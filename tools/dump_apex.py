"""
Apex Legends Memory Dumper + Offset Scanner
Uses kModeKernel.sys shared memory IPC to read decrypted game binary.

Usage:
  1. Load driver (kdmapper)
  2. Launch Apex Legends
  3. Run: python dump_apex.py

Outputs:
  - r5apex_dump.bin  (full decrypted binary)
  - offsets_updated.h (ready to paste into engine/impl/offsets.h)
"""

import ctypes
import ctypes.wintypes as wt
import struct
import sys
import os
import time

# ============================================================
# Win32 API
# ============================================================
kernel32 = ctypes.windll.kernel32
ntdll = ctypes.windll.ntdll

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
# Driver IPC
# ============================================================
class DriverClient:
    def __init__(self):
        self.hMapRead = None
        self.hMapWrite = None
        self._hook_fn = None

        # Load hook function
        win32u = ctypes.windll.LoadLibrary("win32u.dll")
        self._hook_fn = ctypes.cast(
            kernel32.GetProcAddress(
                kernel32.GetModuleHandleA(b"win32u.dll"),
                b"NtDxgkGetTrackedWorkloadStatistics"
            ),
            ctypes.CFUNCTYPE(ctypes.c_uint64)
        )

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
        # Clear
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
# Process finder
# ============================================================
def find_process(name: str) -> int:
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snap == -1:
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

# ============================================================
# PE Parser for in-memory image
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

    # Read optional header to get SizeOfImage
    opt_header = drv.read(base + e_lfanew + 0x18, opt_header_size)
    size_of_image = struct.unpack('<I', opt_header[56:60])[0]

    # Read section headers
    sec_table_offset = e_lfanew + 0x18 + opt_header_size
    sec_data = drv.read(base + sec_table_offset, num_sections * 40)

    sections = []
    for i in range(num_sections):
        off = i * 40
        name = sec_data[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
        vsize = struct.unpack('<I', sec_data[off+8:off+12])[0]
        vaddr = struct.unpack('<I', sec_data[off+12:off+16])[0]
        raw_size = struct.unpack('<I', sec_data[off+16:off+20])[0]
        raw_addr = struct.unpack('<I', sec_data[off+20:off+24])[0]
        chars = struct.unpack('<I', sec_data[off+36:off+40])[0]
        sections.append({
            'name': name,
            'vsize': vsize,
            'vaddr': vaddr,
            'raw_size': raw_size,
            'raw_addr': raw_addr,
            'chars': chars,
        })
        print(f"  {name:10s}  VA={hex(vaddr):12s}  VSize={hex(vsize):12s}  Chars={hex(chars)}")

    return {'sections': sections, 'size_of_image': size_of_image}

# ============================================================
# Memory dumper
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
# Signature scanner
# ============================================================
def sig_scan(data, pattern, mask=None):
    """Simple pattern scanner. Pattern is bytes, mask is string of 'x' and '?'"""
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
    """Resolve a RIP-relative LEA/MOV: addr = offset + instr_len + disp32"""
    disp = struct.unpack('<i', data[offset+3:offset+7])[0]
    return offset + instr_len + disp

# Known signatures for Apex Legends offsets
# Format: (name, pattern_bytes, mask, rip_offset_in_match, instruction_length)
SIGNATURES = [
    # EntityList: 48 8B 1C C8 48 85 DB (pattern near entity list usage)
    # Better: LEA to entity list
    ("OFFSET_ENTITYLIST",
     b"\x48\x8D\x05\x00\x00\x00\x00\x48\x89\x44\x24\x00\x4C\x8B",
     "xxx????xxxx?xx", 0, 7),

    # LocalPlayer: 48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 40
    ("OFFSET_LOCAL_PLAYER",
     b"\x48\x8B\x05\x00\x00\x00\x00\x48\x85\xC0\x74\x00\x48\x8B\x40",
     "xxx????xxxx?xxx", 0, 7),

    # ViewRender: 48 8B 0D ?? ?? ?? ?? 48 8B 01 FF 50 ?? 48 8B
    ("OFFSET_RENDER",
     b"\x48\x8B\x0D\x00\x00\x00\x00\x48\x8B\x01\xFF\x50",
     "xxx????xxxxx", 0, 7),

    # ViewMatrix: offset within ViewRender struct, scanned separately

    # NameList: 48 8D 05 ?? ?? ?? ?? 4C 8B ?? 48
    ("OFFSET_NAME_LIST",
     b"\x48\x8D\x05\x00\x00\x00\x00\x4C\x8B",
     "xxx????xx", 0, 7),
]

def scan_offsets(dump_path):
    """Scan dumped binary for known offset signatures."""
    print(f"\n[*] Scanning {dump_path} for signatures...")

    with open(dump_path, 'rb') as f:
        data = f.read()

    print(f"[*] Loaded {len(data)} bytes ({len(data)/1024/1024:.1f} MB)")

    found = {}

    for name, pattern, mask, rip_off, instr_len in SIGNATURES:
        results = sig_scan(data, pattern, mask)
        if results:
            # Take first match and resolve RIP-relative
            addr = results[0]
            resolved = resolve_rip_relative(data, addr + rip_off, instr_len)
            found[name] = resolved
            print(f"  [+] {name}: {hex(resolved)} (sig at {hex(addr)}, {len(results)} matches)")
        else:
            print(f"  [-] {name}: NOT FOUND")

    # Also try to find some offsets by scanning for known constant patterns
    # Health offset: look for comparisons with 100 (default max health)
    # Team offset: sequential read pattern near health

    return found

def scan_convar_offsets(data):
    """Scan for convar string references to find convar value addresses."""
    convars_to_find = {
        b"thirdperson_override": "THIRDPERSON",
        b"host_timescale": "TIMESCALE",
        b"mat_fullbright": "FULLBRIGHT",
    }

    found = {}
    for cvar_name, label in convars_to_find.items():
        idx = data.find(cvar_name)
        if idx != -1:
            print(f"  [+] String '{cvar_name.decode()}' found at {hex(idx)}")
            found[label] = idx
        else:
            print(f"  [-] String '{cvar_name.decode()}' not found")

    return found

# ============================================================
# Generate offsets.h
# ============================================================
def generate_offsets_h(found_offsets, output_path):
    """Generate updated offsets.h from scanned results."""

    # Start with the known offsets, update with found ones
    lines = [
        '#pragma once',
        '#include <cstdint>',
        '',
        f'// Apex Legends offsets -- auto-dumped from memory',
        f'// Game version: scanned at {time.strftime("%Y-%m-%d %H:%M")}',
        '',
        'namespace offsets',
        '{',
    ]

    if 'OFFSET_ENTITYLIST' in found_offsets:
        lines.append(f'\tconstexpr uintptr_t entity_list = {hex(found_offsets["OFFSET_ENTITYLIST"])};')
    if 'OFFSET_LOCAL_PLAYER' in found_offsets:
        lines.append(f'\tconstexpr uintptr_t local_player = {hex(found_offsets["OFFSET_LOCAL_PLAYER"])};')
    if 'OFFSET_RENDER' in found_offsets:
        lines.append(f'\tconstexpr uintptr_t view_render = {hex(found_offsets["OFFSET_RENDER"])};')
    if 'OFFSET_NAME_LIST' in found_offsets:
        lines.append(f'\tconstexpr uintptr_t name_list = {hex(found_offsets["OFFSET_NAME_LIST"])};')

    lines.append('')
    lines.append('\t// These struct offsets rarely change between patches:')
    lines.append('\tconstexpr uintptr_t view_matrix = 0x11A350;')
    lines.append('\tconstexpr uintptr_t health = 0x0328;')
    lines.append('\tconstexpr uintptr_t max_health = 0x0468;')
    lines.append('\tconstexpr uintptr_t sheild = 0x01a0;')
    lines.append('\tconstexpr uintptr_t max_sheild = 0x01a4;')
    lines.append('\tconstexpr uintptr_t team_id = 0x0338;')
    lines.append('\tconstexpr uintptr_t life_state = 0x0690;')
    lines.append('\tconstexpr uintptr_t bleedout_state = 0x2760;')
    lines.append('\tconstexpr uintptr_t origin = 0x017c;')
    lines.append('\tconstexpr uintptr_t bone_array = 0x0da8 + 0x48;')
    lines.append('\tconstexpr uintptr_t studiohdr = 0xff0;')
    lines.append('\tconstexpr uintptr_t last_active = 0x1934;')
    lines.append('\tconstexpr uintptr_t skin_id = 0x0d68;')
    lines.append('\tconstexpr uintptr_t view_model = 0x2e00;')
    lines.append('\tconstexpr uintptr_t camera_pos = 0x1ee0;')
    lines.append('\tconstexpr uintptr_t view_angles = 0x2534 - 0x14;')
    lines.append('\tconstexpr uintptr_t aimpunch = 0x2438;')
    lines.append('\tconstexpr uintptr_t visible_time = 0x1990;')
    lines.append('\tconstexpr uintptr_t glow_enable = 0x28C;')
    lines.append('\tconstexpr uintptr_t glow_through_walls = 0x26c;')
    lines.append('\tconstexpr uintptr_t glow_t1 = 0x292;')
    lines.append('}')

    content = '\n'.join(lines) + '\n'
    with open(output_path, 'w') as f:
        f.write(content)
    print(f"\n[+] Generated: {output_path}")

# ============================================================
# Main
# ============================================================
def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    dump_path = os.path.join(script_dir, "r5apex_dump.bin")
    offsets_path = os.path.join(script_dir, "offsets_updated.h")

    # If dump already exists, just scan it
    if os.path.exists(dump_path) and "--force" not in sys.argv:
        print(f"[*] Found existing dump: {dump_path}")
        print(f"    Use --force to re-dump from memory")
        found = scan_offsets(dump_path)
        with open(dump_path, 'rb') as f:
            data = f.read()
        convars = scan_convar_offsets(data)
        found.update(convars)
        generate_offsets_h(found, offsets_path)
        return

    # Step 1: Find game process
    proc_names = ["r5apex.exe", "r5apex_dx12.exe"]
    pid = 0
    proc_name = ""
    for name in proc_names:
        pid = find_process(name)
        if pid:
            proc_name = name
            break

    if not pid:
        print("[-] Apex Legends not running!")
        print("    Start the game first, then run this script.")
        sys.exit(1)

    print(f"[+] Found {proc_name} (PID: {pid})")

    # Step 2: Connect to kernel driver
    drv = DriverClient()
    if not drv.connect():
        print("[-] Cannot connect to driver. Is kModeKernel.sys loaded?")
        sys.exit(1)
    print("[+] Driver connection established")

    # Step 3: Attach to game
    drv.attach(proc_name.encode())
    print(f"[+] Attached to {proc_name}")

    # Step 4: Get module base
    base = drv.get_base(proc_name.encode())
    if not base:
        print("[-] Failed to get module base")
        sys.exit(1)
    print(f"[+] Module base: {hex(base)}")

    # Verify MZ header
    mz = drv.read_u16(base)
    if mz != 0x5A4D:
        print(f"[-] MZ header mismatch: {hex(mz)}")
        sys.exit(1)
    print("[+] MZ header OK")

    # Step 5: Parse PE and dump
    pe_info = parse_pe_sections(drv, base)
    if not pe_info:
        sys.exit(1)

    dump_image(drv, base, pe_info['size_of_image'], dump_path)

    # Step 6: Scan for offsets
    found = scan_offsets(dump_path)
    with open(dump_path, 'rb') as f:
        data = f.read()
    convars = scan_convar_offsets(data)
    found.update(convars)
    generate_offsets_h(found, offsets_path)

    print("\n[+] Done! Review offsets_updated.h and copy to engine/impl/offsets.h")

if __name__ == "__main__":
    main()
