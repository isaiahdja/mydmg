import ctypes
import json
import glob

def make_u16(hi, lo):
    return (hi << 8) | lo

def get_lo(u16):
    return u16 & 0xFF

def get_hi(u16):
    return (u16 >> 8) & 0xFF

class CPUState (ctypes.Structure):
    _fields_ = [
        ("af_reg", ctypes.c_uint16),
        ("bc_reg", ctypes.c_uint16),
        ("de_reg", ctypes.c_uint16),
        ("hl_reg", ctypes.c_uint16),
        ("sp_reg", ctypes.c_uint16),
        ("pc_reg", ctypes.c_uint16),

        ("ime_flag", ctypes.c_bool),
    ]

MemoryArrayType = ctypes.c_uint8 * (1 << 16)
memory = MemoryArrayType()

def py_read(addr):
    return memory[addr]
ReadType = ctypes.CFUNCTYPE(ctypes.c_uint8, ctypes.c_uint16)
read_cb = ReadType(py_read)

def py_write(addr, val):
    memory[addr] = val
WriteType = ctypes.CFUNCTYPE(None, ctypes.c_uint16, ctypes.c_uint8)
write_cb = WriteType(py_write)

def py_pending_interrupt():
    return False
PendingIntType = ctypes.CFUNCTYPE(
    ctypes.c_bool)
pending_interrupt_cb = PendingIntType(py_pending_interrupt)

def py_receive_interrupt(jump_vec_ptr):
    return False
ReceiveIntType = ctypes.CFUNCTYPE(
    ctypes.c_bool, ctypes.POINTER(ctypes.c_uint16))
receive_interrupt_cb = ReceiveIntType(py_receive_interrupt)

lib = ctypes.CDLL("build-debug/libcpu_test.so")
lib.cpu_test_init.restype = ctypes.c_bool
lib.cpu_test_init.argtypes = [ReadType, WriteType, PendingIntType, ReceiveIntType]
lib.cpu_test_get_state.restype = CPUState
lib.cpu_test_set_state.argtypes = [CPUState]

lib.cpu_test_init(read_cb, write_cb, pending_interrupt_cb, receive_interrupt_cb)

passed = 0
total = 0
for path in glob.glob("test/sm83/v1/??.json"):
    with open(path) as f:
        tests = json.load(f)

    for test in tests:
        total += 1
        test_name = test["name"]

        init = test["initial"]

        for addr, val in init["ram"]:
            py_write(addr, val)

        init_set = CPUState(
            make_u16(init["a"], init["f"]),
            make_u16(init["b"], init["c"]),
            make_u16(init["d"], init["e"]),
            make_u16(init["h"], init["l"]),
            init["sp"],
            init["pc"],

            init["ime"]
        )
        lib.cpu_test_set_state(init_set)
        
        for _ in test["cycles"]:
            lib.cpu_tick()

        final = test["final"]
        # Account for our fetch/execute overlap.
        final["pc"] += 1
        if (final["pc"] == 65536):
            final["pc"] = 0
        final_get = lib.cpu_test_get_state()
        checks = [
            ("a", final["a"], get_hi(final_get.af_reg)),
            ("f", final["f"], get_lo(final_get.af_reg)),
            ("b", final["b"], get_hi(final_get.bc_reg)),
            ("c", final["c"], get_lo(final_get.bc_reg)),
            ("d", final["d"], get_hi(final_get.de_reg)),
            ("e", final["e"], get_lo(final_get.de_reg)),
            ("h", final["h"], get_hi(final_get.hl_reg)),
            ("l", final["l"], get_lo(final_get.hl_reg)),
            ("sp", final["sp"], final_get.sp_reg),
            ("pc", final["pc"], final_get.pc_reg),
            ("ime", final["ime"], final_get.ime_flag)
        ]

        for addr, val in final["ram"]:
            checks.append((str(addr), val, py_read(addr)))

        success = True
        for name, expected, actual in checks:
            if (expected != actual):
                if success:
                    print(f"{test_name}: FAILED")
                success = False
                print(f"\t{name}: Expected = {bin(expected)}; Actual = {bin(actual)}")
        
        if success:
            passed += 1
            print(f"{test_name}: PASSED")

print(f"Passed: {passed}/{total}")