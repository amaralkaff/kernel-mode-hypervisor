"""
Apex Legends All-in-One Loader + Offset Dumper
===============================================
Single script that:
  1. Loads kModeKernel.sys via KDMapper (if not already loaded)
  2. Waits for Apex Legends to launch
  3. Attaches via kernel driver shared memory IPC
  4. Dumps decrypted game binary from memory
  5. Scans for global pointer offsets
  6. Generates offsets_updated.h

Usage:
  python apex_loader.py
  python apex_loader.py --force     (re-dump even if dump file exists)
  python apex_loader.py --no-dump   (just load driver + wait for game, skip dump)

Requirements:
  - Run as Administrator
  - kdmapper_Release.exe + kModeKernel.sys in same folder (or parent folder)
  - Apex Legends installed
"""

import ctypes
import ctypes.wintypes as wt
import struct
import subprocess
import sys
import os
import time
import glob

# ============================================================
# Win32 API
# ============================================================
kernel32 = ctypes.windll.kernel32
shell32 = ctypes.windll.shell32

OpenFileMappingA = kernel32.OpenFileMappingA
OpenFileMappingA.restype = wt.HANDLE
OpenFileMappingA.argtypes = [wt.DWORD, wt.BOOL, ctypes.c_char_p]

MapViewOfFile = kernel32.MapViewOfFile
MapViewOfFile.restype = ctypes.c_void_p
MapViewOfFile.argtypes = [wt.HANDLE, wt.DWORD, wt.DWORD, wt.DWORD, ctypes.c_size_t]

UnmapViewOfFile = kernel32.UnmapViewOfFile
UnmapViewOfFile.restype = wt.BOOL
UnmapViewOfFile.argtypes = [ctypes.c_void_p]

CloseHandle = kernel32.CloseHandle

CreateToolhelp32Snapshot = kernel32.CreateToolhelp32Snapshot
Process32FirstW = kernel32.Process32FirstW
Process32NextW = kernel32.Process32NextW

FILE_MAP_READ = 0x0004
FILE_MAP_WRITE = 0x0002
TH32CS_SNAPPROCESS = 0x00000002

class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD),
        ("cntUsage", wt.DWORD),
        ("th32ProcessID", wt.DWORD),
        ("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)),
        ("th32ModuleID", wt.DWORD),
        ("cntThreads", wt.DWORD),
        ("th32ParentProcessID", wt.DWORD),
        ("pcPriClassBase", ctypes.c_long),
        ("dwFlags", wt.DWORD),
        ("szExeFile", ctypes.c_wchar * 260),
    ]

# ============================================================
# copy_memory struct (must match kernel driver)
# ============================================================
class CopyMemory(ctypes.Structure):
    _fields_ = [
        ("called", ctypes.c_ubyte),
        ("read", ctypes.c_ubyte),
        ("read_string", ctypes.c_ubyte),
        ("buffer_address", ctypes.c_void_p),
        ("address", ctypes.c_uint64),
        ("size", ctypes.c_uint64),
        ("output", ctypes.c_void_p),

        ("write", ctypes.c_ubyte),
        ("write_string", ctypes.c_ubyte),

        ("get_base", ctypes.c_ubyte),
        ("base_address", ctypes.c_uint64),
        ("module_name", ctypes.c_char_p),

        ("get_pid", ctypes.c_ubyte),
        ("process_name", ctypes.c_char_p),
        ("pid_of_source", ctypes.c_uint32),

        ("alloc_memory", ctypes.c_ubyte),
        ("alloc_type", ctypes.c_uint32),

        ("change_protection", ctypes.c_ubyte),
        ("protection", ctypes.c_uint32),
        ("protection_old", ctypes.c_uint32),

        ("get_thread_context", ctypes.c_ubyte),
        ("set_thread_context", ctypes.c_ubyte),

        ("end", ctypes.c_ubyte),

        ("window_handle", wt.HWND),
        ("thread_context", ctypes.c_uint64),
    ]

# ============================================================
# Helpers
# ============================================================
def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

def find_process(name: str) -> int:
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snap == -1 or snap == 0xFFFFFFFF:
        return 0
    pe = PROCESSENTRY32W()
    pe.dwSize = ctypes.sizeof(PROCESSENTRY32W)
    if Process32FirstW(snap, ctypes.byref(pe)):
        while True:
            if pe.szExeFile.lower() == name.lower():
                pid = pe.th32ProcessID
                CloseHandle(snap)
                return pid
            if not Process32NextW(snap, ctypes.byref(pe)):
                break
    CloseHandle(snap)
    return 0

def driver_is_loaded():
    h = OpenFileMappingA(FILE_MAP_READ, False, b"Global\\DxGrfx_SharedView")
    if h and h != 0xFFFFFFFF:
        CloseHandle(h)
        return True
    return False

def find_file(filename, search_dirs):
    """Search for a file in multiple directories."""
    for d in search_dirs:
        path = os.path.join(d, filename)
        if os.path.exists(path):
            return path
    return None

# ============================================================
# Phase 1: Driver Loading
# ============================================================
def load_driver():
    """Load kModeKernel.sys via KDMapper."""
    if driver_is_loaded():
        print("[+] Driver already loaded")
        return True

    script_dir = os.path.dirname(os.path.abspath(__file__))
    parent_dir = os.path.dirname(script_dir)

    docs_dir = os.path.join(os.path.expanduser("~"), "Documents")
    search_dirs = [
        script_dir,
        parent_dir,
        os.path.join(parent_dir, "dist"),
        os.path.join(parent_dir, "build"),
        os.path.join(parent_dir, "x64", "Release"),
        os.getcwd(),
        # Known build output locations
        os.path.join(docs_dir, "kdmapper", "x64", "Release"),
        os.path.join(docs_dir, "asd", "x64", "Release"),
        os.path.join(docs_dir, "asd"),
        os.path.join(docs_dir, "kdmapper"),
    ]

    kdmapper = find_file("kdmapper_Release.exe", search_dirs)
    if not kdmapper:
        kdmapper = find_file("kdmapper.exe", search_dirs)
    if not kdmapper:
        print("[-] kdmapper not found! Searched:")
        for d in search_dirs:
            print(f"    {d}")
        return False

    sys_file = find_file("kModeKernel.sys", search_dirs)
    if not sys_file:
        print("[-] kModeKernel.sys not found! Searched:")
        for d in search_dirs:
            print(f"    {d}")
        return False

    print(f"[*] KDMapper: {kdmapper}")
    print(f"[*] Driver:   {sys_file}")
    print("[*] Loading driver...")

    try:
        # Run KDMapper silently
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startupinfo.wShowWindow = 0  # SW_HIDE

        proc = subprocess.Popen(
            [kdmapper, sys_file],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            startupinfo=startupinfo,
        )
        stdout, stderr = proc.communicate(timeout=30)

        if proc.returncode != 0:
            print(f"[-] KDMapper exited with code {proc.returncode}")
            if stdout:
                print(f"    stdout: {stdout.decode(errors='replace')}")
            if stderr:
                print(f"    stderr: {stderr.decode(errors='replace')}")
    except subprocess.TimeoutExpired:
        proc.kill()
        print("[-] KDMapper timed out")
    except Exception as e:
        print(f"[-] Failed to run KDMapper: {e}")

    # Wait for shared memory to appear
    for i in range(15):
        if driver_is_loaded():
            print("[+] Driver loaded successfully")
            return True
        time.sleep(1)
        print(f"[*] Waiting for driver... ({i+1}/15)")

    print("[-] Driver load failed - shared memory not found")
    return False

# ============================================================
# Phase 2: Wait for Game
# ============================================================
def wait_for_game():
    """Wait for Apex Legends to launch."""
    proc_names = ["r5apex.exe", "r5apex_dx12.exe"]

    # Check if already running
    for name in proc_names:
        pid = find_process(name)
        if pid:
            print(f"[+] {name} already running (PID: {pid})")
            return name, pid

    # Wait for it
    print("[*] Waiting for Apex Legends to launch...")
    spinner = ['|', '/', '-', '\\']
    i = 0
    while True:
        for name in proc_names:
            pid = find_process(name)
            if pid:
                print(f"\r[+] {name} detected (PID: {pid})              ")
                print("[*] Waiting 10s for modules to load...")
                time.sleep(10)
                return name, pid
        print(f"\r[*] Waiting for Apex... {spinner[i % 4]}  ", end='', flush=True)
        i += 1
        time.sleep(2)

# ============================================================
# Driver IPC Client
# ============================================================
class DriverClient:
    def __init__(self):
        self.hMapRead = None
        self.hMapWrite = None
        self._hook_fn = None

        # Load hook trigger function
        try:
            ctypes.windll.LoadLibrary("win32u.dll")
            addr = kernel32.GetProcAddress(
                kernel32.GetModuleHandleA(b"win32u.dll"),
                b"NtDxgkGetTrackedWorkloadStatistics"
            )
            if addr:
                self._hook_fn = ctypes.cast(addr, ctypes.CFUNCTYPE(ctypes.c_uint64))
        except:
            pass

    def connect(self):
        self.hMapRead = OpenFileMappingA(FILE_MAP_READ, False, b"Global\\DxGrfx_SharedView")
        if not self.hMapRead:
            return False
        self.hMapWrite = OpenFileMappingA(FILE_MAP_WRITE, False, b"Global\\DxGrfx_SharedView")
        if not self.hMapWrite:
            return False
        return True

    def _call_hook(self):
        if self._hook_fn:
            self._hook_fn()

    def attach(self, process_name: bytes):
        m = CopyMemory()
        m.called = 1
        m.get_pid = 1
        m.process_name = process_name

        view = MapViewOfFile(self.hMapWrite, FILE_MAP_WRITE, 0, 0, 4096)
        if not view:
            return False
        ctypes.memmove(view, ctypes.byref(m), ctypes.sizeof(m))
        self._call_hook()
        ctypes.memset(view, 0, ctypes.sizeof(CopyMemory))
        UnmapViewOfFile(view)
        return True

    def get_base(self, module_name: bytes) -> int:
        m = CopyMemory()
        m.called = 1
        m.get_base = 1
        m.module_name = module_name

        view = MapViewOfFile(self.hMapWrite, FILE_MAP_WRITE, 0, 0, 4096)
        if not view:
            return 0
        ctypes.memmove(view, ctypes.byref(m), ctypes.sizeof(m))
        self._call_hook()

        result_view = MapViewOfFile(self.hMapRead, FILE_MAP_READ, 0, 0, ctypes.sizeof(CopyMemory))
        if not result_view:
            UnmapViewOfFile(view)
            return 0

        result = CopyMemory()
        ctypes.memmove(ctypes.byref(result), result_view, ctypes.sizeof(CopyMemory))
        base = result.base_address
        UnmapViewOfFile(result_view)

        ctypes.memset(view, 0, ctypes.sizeof(CopyMemory))
        UnmapViewOfFile(view)
        return base

    def read(self, address: int, size: int) -> bytes:
        buf = (ctypes.c_ubyte * size)()

        m = CopyMemory()
        m.called = 1
        m.read = 1
        m.address = address
        m.size = size
        m.output = ctypes.cast(buf, ctypes.c_void_p)

        view = MapViewOfFile(self.hMapWrite, FILE_MAP_WRITE, 0, 0, 4096)
        if not view:
            return b'\x00' * size
        ctypes.memmove(view, ctypes.byref(m), ctypes.sizeof(m))
        self._call_hook()
        ctypes.memset(view, 0, ctypes.sizeof(CopyMemory))
        UnmapViewOfFile(view)

        return bytes(buf)

    def read_u16(self, addr): return struct.unpack('<H', self.read(addr, 2))[0]
    def read_u32(self, addr): return struct.unpack('<I', self.read(addr, 4))[0]
    def read_u64(self, addr): return struct.unpack('<Q', self.read(addr, 8))[0]

# ============================================================
# PE Parser
# ============================================================
def parse_pe_sections(drv, base):
    """Parse PE headers from memory and return section info."""
    dos_header = drv.read(base, 0x40)
    if dos_header[:2] != b'MZ':
        print("[-] Not a valid PE (no MZ header)")
        return None

    e_lfanew = struct.unpack('<I', dos_header[0x3C:0x40])[0]
    pe_header = drv.read(base + e_lfanew, 0x18)
    if pe_header[:4] != b'PE\x00\x00':
        print("[-] Invalid PE signature")
        return None

    num_sections = struct.unpack('<H', pe_header[6:8])[0]
    opt_header_size = struct.unpack('<H', pe_header[20:22])[0]

    opt_header = drv.read(base + e_lfanew + 0x18, opt_header_size)
    size_of_image = struct.unpack('<I', opt_header[56:60])[0]

    sec_table_offset = e_lfanew + 0x18 + opt_header_size
    sec_data = drv.read(base + sec_table_offset, num_sections * 40)

    sections = []
    for i in range(num_sections):
        off = i * 40
        name = sec_data[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
        vsize = struct.unpack('<I', sec_data[off+8:off+12])[0]
        vaddr = struct.unpack('<I', sec_data[off+12:off+16])[0]
        chars = struct.unpack('<I', sec_data[off+36:off+40])[0]
        sections.append({
            'name': name, 'vsize': vsize, 'vaddr': vaddr, 'chars': chars,
        })
        print(f"  {name:10s}  VA={hex(vaddr):12s}  VSize={hex(vsize):12s}")

    return {'sections': sections, 'size_of_image': size_of_image}

# ============================================================
# Memory Dumper
# ============================================================
def dump_image(drv, base, size_of_image, output_path, chunk_size=0x10000):
    """Dump full in-memory PE image to file."""
    print(f"[*] Dumping {hex(size_of_image)} bytes from {hex(base)}...")
    total = size_of_image
    dumped = 0

    with open(output_path, 'wb') as f:
        while dumped < total:
            remaining = total - dumped
            read_size = min(chunk_size, remaining)
            data = drv.read(base + dumped, read_size)
            f.write(data)
            dumped += read_size
            pct = (dumped / total) * 100
            print(f"\r[*] Progress: {pct:.1f}% ({hex(dumped)}/{hex(total)})", end='', flush=True)

    print(f"\n[+] Dump saved to: {output_path}")

# ============================================================
# Signature Scanner
# ============================================================
def sig_scan(data, pattern, mask=None):
    results = []
    plen = len(pattern)
    for i in range(len(data) - plen):
        found = True
        for j in range(plen):
            if mask and mask[j] == '?':
                continue
            if data[i + j] != pattern[j]:
                found = False
                break
        if found:
            results.append(i)
    return results

def resolve_rip_relative(data, offset, instr_len=7):
    disp = struct.unpack('<i', data[offset+3:offset+7])[0]
    return offset + instr_len + disp

SIGNATURES = [
    ("OFFSET_ENTITYLIST",
     b"\x48\x8D\x05\x00\x00\x00\x00\x48\x89\x44\x24\x00\x4C\x8B",
     "xxx????xxxx?xx", 0, 7),

    ("OFFSET_LOCAL_PLAYER",
     b"\x48\x8B\x05\x00\x00\x00\x00\x48\x85\xC0\x74\x00\x48\x8B\x40",
     "xxx????xxxx?xxx", 0, 7),

    ("OFFSET_RENDER",
     b"\x48\x8B\x0D\x00\x00\x00\x00\x48\x8B\x01\xFF\x50",
     "xxx????xxxxx", 0, 7),

    ("OFFSET_NAME_LIST",
     b"\x48\x8D\x05\x00\x00\x00\x00\x4C\x8B",
     "xxx????xx", 0, 7),
]

def scan_offsets(dump_path):
    print(f"\n[*] Scanning {dump_path} for signatures...")
    with open(dump_path, 'rb') as f:
        data = f.read()
    print(f"[*] Loaded {len(data)} bytes ({len(data)/1024/1024:.1f} MB)")

    found = {}
    for name, pattern, mask, rip_off, instr_len in SIGNATURES:
        results = sig_scan(data, pattern, mask)
        if results:
            addr = results[0]
            resolved = resolve_rip_relative(data, addr + rip_off, instr_len)
            found[name] = resolved
            print(f"  [+] {name}: {hex(resolved)} (sig at {hex(addr)}, {len(results)} matches)")
        else:
            print(f"  [-] {name}: NOT FOUND")

    return found

# ============================================================
# Generate offsets.h
# ============================================================
def generate_offsets_h(found_offsets, output_path):
    lines = [
        '#pragma once',
        '#include <cstdint>',
        '',
        f'// Apex Legends offsets -- auto-dumped from memory',
        f'// Scanned at {time.strftime("%Y-%m-%d %H:%M")}',
        '//',
        '// Global pointers were resolved from in-memory signatures.',
        '// Struct offsets are from apexsky v3.0.82.42 (stable across patches).',
        '',
        'namespace offsets',
        '{',
        '\t// ====== GLOBAL POINTERS (dumped from your game version) ======',
    ]

    if 'OFFSET_ENTITYLIST' in found_offsets:
        lines.append(f'\tconstexpr uintptr_t entity_list = {hex(found_offsets["OFFSET_ENTITYLIST"])};')
    else:
        lines.append(f'\tconstexpr uintptr_t entity_list = 0x1f62278;       // FALLBACK - apexsky v3.0.82.42')

    if 'OFFSET_LOCAL_PLAYER' in found_offsets:
        lines.append(f'\tconstexpr uintptr_t local_player = {hex(found_offsets["OFFSET_LOCAL_PLAYER"])};')
    else:
        lines.append(f'\tconstexpr uintptr_t local_player = 0x24354F8;      // FALLBACK - apexsky v3.0.82.42')

    if 'OFFSET_RENDER' in found_offsets:
        lines.append(f'\tconstexpr uintptr_t view_render = {hex(found_offsets["OFFSET_RENDER"])};')
    else:
        lines.append(f'\tconstexpr uintptr_t view_render = 0x76e9ab8;       // FALLBACK - apexsky v3.0.82.42')

    if 'OFFSET_NAME_LIST' in found_offsets:
        lines.append(f'\tconstexpr uintptr_t name_list = {hex(found_offsets["OFFSET_NAME_LIST"])};')
    else:
        lines.append(f'\tconstexpr uintptr_t name_list = 0xd427360;         // FALLBACK - apexsky v3.0.82.42')

    lines.append(f'\tconstexpr uintptr_t model_name = name_list;')
    lines.append(f'\tconstexpr uintptr_t highlight_settings = 0xb1db5a0;')
    lines.append('')
    lines.append('\t// ====== VIEW MATRIX (inside ViewRender struct, stable) ======')
    lines.append('\tconstexpr uintptr_t view_matrix = 0x11A350;')
    lines.append('')
    lines.append('\t// ====== ENTITY STRUCT OFFSETS (stable across patches) ======')
    lines.append('\tconstexpr uintptr_t health = 0x0328;               // m_iHealth')
    lines.append('\tconstexpr uintptr_t max_health = 0x0470;            // m_iMaxHealth')
    lines.append('\tconstexpr uintptr_t sheild = 0x01a0;               // m_shieldHealth')
    lines.append('\tconstexpr uintptr_t max_sheild = 0x01a4;           // m_shieldHealthMax')
    lines.append('\tconstexpr uintptr_t team_id = 0x0338;              // m_iTeamNum')
    lines.append('\tconstexpr uintptr_t life_state = 0x0690;           // m_lifeState')
    lines.append('\tconstexpr uintptr_t bleedout_state = 0x2760;       // m_bleedoutState')
    lines.append('\tconstexpr uintptr_t origin = 0x017c;               // m_vecAbsOrigin')
    lines.append('\tconstexpr uintptr_t abs_velocity = 0x0170;         // m_vecAbsVelocity')
    lines.append('')
    lines.append('\t// ====== BONES / MODEL ======')
    lines.append('\tconstexpr uintptr_t bone_array = 0x0db0 + 0x48;   // m_nForceBone + 0x48')
    lines.append('\tconstexpr uintptr_t studiohdr = 0x1000;            // CBaseAnimating!m_pStudioHdr')
    lines.append('')
    lines.append('\t// ====== WEAPON ======')
    lines.append('\tconstexpr uintptr_t last_active = 0x1944;          // m_latestPrimaryWeapons')
    lines.append('\tconstexpr uintptr_t skin_id = 0x0d68;              // weapon skin ID')
    lines.append('\tconstexpr uintptr_t view_model = 0x2d98;           // m_hViewModels')
    lines.append('\tconstexpr uintptr_t bullet_speed = 0x19d8 + 0x04ec; // CWeaponX!m_flProjectileSpeed')
    lines.append('\tconstexpr uintptr_t bullet_scale = 0x19d8 + 0x04f4; // CWeaponX!m_flProjectileScale')
    lines.append('\tconstexpr uintptr_t zoom_fov = 0x15e0 + 0x00b8;   // m_playerData + m_curZoomFOV')
    lines.append('\tconstexpr uintptr_t ammo = 0x1590;                 // m_ammoInClip')
    lines.append('')
    lines.append('\t// ====== AIM / VIEW ======')
    lines.append('\tconstexpr uintptr_t aimpunch = 0x2438;             // m_currentFrameLocalPlayer.m_vecPunchWeapon_Angle')
    lines.append('\tconstexpr uintptr_t camera_pos = 0x1ee0;           // CPlayer!camera_origin')
    lines.append('\tconstexpr uintptr_t view_angles = 0x2534 - 0x14;  // m_ammoPoolCapacity - 0x14')
    lines.append('\tconstexpr uintptr_t breath_angles = view_angles - 0x10;')
    lines.append('\tconstexpr uintptr_t visible_time = 0x19a0;         // CPlayer!lastVisibleTime')
    lines.append('\tconstexpr uintptr_t zooming = 0x1be1;              // m_bZooming')
    lines.append('\tconstexpr uintptr_t yaw = 0x223c - 0x8;            // m_currentFramePlayer.m_ammoPoolCount - 0x8')
    lines.append('')
    lines.append('\t// ====== OBSERVER ======')
    lines.append('\tconstexpr uintptr_t observer_mode = 0x3584;        // m_iObserverMode')
    lines.append('\tconstexpr uintptr_t observing_target = 0x3590;     // m_hObserverTarget')
    lines.append('')
    lines.append('\t// ====== GLOW / HIGHLIGHT ======')
    lines.append('\tconstexpr uintptr_t item_glow = 0x02f0;            // m_highlightFunctionBits')
    lines.append('\tconstexpr uintptr_t glow_enable = 0x28C;           // 7=enabled, 2=disabled')
    lines.append('\tconstexpr uintptr_t glow_through_walls = 0x26c;    // 2=enabled, 5=disabled')
    lines.append('\tconstexpr uintptr_t glow_fix = 0x278;              // glow fix')
    lines.append('\tconstexpr uintptr_t glow_context_id = 0x29c;       // glow context ID')
    lines.append('\tconstexpr uintptr_t glow_t1 = 0x292;               // 16256=enabled, 0=disabled')
    lines.append('\tconstexpr uintptr_t glow_t2 = 0x30c;               // 1193322764=enabled, 0=disabled')
    lines.append('}')

    content = '\n'.join(lines) + '\n'
    with open(output_path, 'w') as f:
        f.write(content)
    print(f"\n[+] Generated: {output_path}")

# ============================================================
# Game monitoring loop
# ============================================================
def monitor_game(proc_name):
    """Monitor if game is still running. Returns when game exits."""
    print(f"\n[*] Monitoring {proc_name}... (Ctrl+C to exit)")
    while True:
        pid = find_process(proc_name)
        if not pid:
            print(f"\n[!] {proc_name} closed")
            return
        time.sleep(5)

# ============================================================
# Main
# ============================================================
def main():
    print("=" * 50)
    print("  Apex Legends Loader + Offset Dumper")
    print("  EAC Bypass via dxgkrnl kernel hook")
    print("=" * 50)
    print()

    # Check admin
    if not is_admin():
        print("[-] Not running as Administrator!")
        print("    Right-click -> Run as Administrator")
        input("\nPress Enter to exit...")
        sys.exit(1)

    force_dump = "--force" in sys.argv
    no_dump = "--no-dump" in sys.argv

    script_dir = os.path.dirname(os.path.abspath(__file__))
    dump_path = os.path.join(script_dir, "r5apex_dump.bin")
    offsets_path = os.path.join(script_dir, "offsets_updated.h")

    # ---- Phase 1: Load driver ----
    print("[Phase 1] Driver")
    if not load_driver():
        print("\n[-] Cannot proceed without driver")
        input("\nPress Enter to exit...")
        sys.exit(1)

    while True:
        # ---- Phase 2: Wait for game ----
        print("\n[Phase 2] Game detection")
        proc_name, pid = wait_for_game()

        # ---- Phase 3: Attach ----
        print("\n[Phase 3] Attaching to game")
        drv = DriverClient()
        if not drv.connect():
            print("[-] Driver connection lost!")
            input("\nPress Enter to exit...")
            sys.exit(1)

        drv.attach(proc_name.encode())
        print(f"[+] Attached to {proc_name}")

        # Get module base
        base = drv.get_base(proc_name.encode())
        if not base:
            # Try the runtime DLL for DX12
            base = drv.get_base(b"r5apex_dx12runtime.dll")
            if base:
                print(f"[+] r5apex_dx12runtime.dll base: {hex(base)}")
            else:
                print("[-] Failed to get module base")
                print("[*] Game may still be loading, retrying in 10s...")
                time.sleep(10)
                base = drv.get_base(proc_name.encode())

        if not base:
            print("[-] Could not get module base after retry")
        else:
            print(f"[+] Module base: {hex(base)}")

            # Verify MZ
            mz = drv.read_u16(base)
            if mz == 0x5A4D:
                print("[+] MZ header verified - kernel read working!")
            else:
                print(f"[!] MZ mismatch: {hex(mz)} (expected 0x5A4D)")

            # ---- Phase 4: Dump + scan offsets ----
            if not no_dump:
                if os.path.exists(dump_path) and not force_dump:
                    print(f"\n[*] Existing dump found: {dump_path}")
                    print(f"    Scanning existing dump (use --force to re-dump)")
                    found = scan_offsets(dump_path)
                    generate_offsets_h(found, offsets_path)
                else:
                    print("\n[Phase 4] Dumping game binary from memory")
                    pe_info = parse_pe_sections(drv, base)
                    if pe_info:
                        dump_image(drv, base, pe_info['size_of_image'], dump_path)
                        found = scan_offsets(dump_path)
                        generate_offsets_h(found, offsets_path)
                    else:
                        print("[-] PE parsing failed, skipping dump")

        # ---- Phase 5: Monitor ----
        try:
            monitor_game(proc_name)
        except KeyboardInterrupt:
            print("\n[*] Exiting...")
            sys.exit(0)

        print("\n[*] Game exited. Waiting for relaunch...")
        print("    (Ctrl+C to quit)\n")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[*] Interrupted. Goodbye!")
        sys.exit(0)
