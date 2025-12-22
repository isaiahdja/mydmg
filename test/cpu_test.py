import ctypes
import json
import glob

class CPUState (ctypes.Structure):
    _fields_ = [
        ("a_reg", ctypes.c_uint8),
        ("z_flag", ctypes.c_bool),
        ("n_flag", ctypes.c_bool),
        ("h_flag", ctypes.c_bool),
        ("c_flag", ctypes.c_bool),
        ("b_reg", ctypes.c_uint8),
        ("c_reg", ctypes.c_uint8),
        ("d_reg", ctypes.c_uint8),
        ("e_reg", ctypes.c_uint8),
        ("h_reg", ctypes.c_uint8),
        ("l_reg", ctypes.c_uint8),
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

def py_receive_interrupt(jump_vec_ptr):
    return False
InterruptType = ctypes.CFUNCTYPE(
    ctypes.c_bool, ctypes.POINTER(ctypes.c_uint16))
receive_interrupt_cb = InterruptType(py_receive_interrupt)

lib = ctypes.CDLL("build-debug/libcpu_test.so")
lib.cpu_test_init.restype = ctypes.c_bool
lib.cpu_test_init.argtypes = [ReadType, WriteType, InterruptType]
lib.cpu_test_get_state.restype = CPUState
lib.cpu_test_set_state.argtypes = [CPUState]

lib.cpu_test_init(read_cb, write_cb, receive_interrupt_cb)

passed = 0
total = 0
for path in glob.glob("test/sm83/v1/00.json"):
    with open(path) as f:
        tests = json.load(f)

    for test in tests:
        total += 1
        test_name = test["name"]

        init = test["initial"]

        memory = MemoryArrayType()
        for addr, val in init["ram"]:
            py_write(addr, val)

        init_state = CPUState(
            init["a"],
            0, 0, 0, 0,
            init["b"], init["c"],
            init["d"], init["e"],
            init["h"], init["l"],
            init["sp"],
            init["pc"],
            init["ime"]
        )
        lib.cpu_test_set_state(init_state)
        
        for _ in test["cycles"]:
            lib.cpu_tick()

        final = test["final"]
        final_observed_state = lib.cpu_test_get_state()
        checks = [
            ("a", final["a"], final_observed_state.a_reg),
            ("b", final["b"], final_observed_state.b_reg),
            ("c", final["c"], final_observed_state.c_reg),
            ("d", final["d"], final_observed_state.d_reg),
            ("e", final["e"], final_observed_state.e_reg),
            ("h", final["h"], final_observed_state.h_reg),
            ("l", final["l"], final_observed_state.l_reg),
            ("sp", final["sp"], final_observed_state.sp_reg),
            ("pc", final["pc"], final_observed_state.pc_reg),
            ("ime", final["ime"], final_observed_state.ime_flag)
        ]

        for addr, val in final["ram"]:
            checks.append((str(addr), val, py_read(addr)))

        success = True
        for name, expected, actual in checks:
            if (expected != actual):
                if success:
                    print(f"{test_name}: FAILED")
                success = False
                print(f"\t{name}: Expected = {expected}; Actual = {actual}")
        
        if success:
            passed += 1
            print(f"{test_name}: PASSED")

print(f"Passed: {passed}/{total}")