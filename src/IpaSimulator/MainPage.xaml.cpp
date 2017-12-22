//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include <LIEF/LIEF.hpp>
#include <LIEF/filesystem/filesystem.h>
#include <sstream>
#include <unicorn/unicorn.h>
#include <memory>

using namespace IpaSimulator;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace std;
using namespace LIEF::MachO;

#if 0
/* Sample code to demonstrate how to emulate ARM code */

// code to be emulated
#define ARM_CODE "\x37\x00\xa0\xe3\x03\x10\x42\xe0" // mov r0, #0x37; sub r1, r2, r3
#define THUMB_CODE "\x83\xb0" // sub    sp, #0xc

// memory address where emulation starts
#define ADDRESS 0x10000

static void hook_block(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    printf(">>> Tracing basic block at 0x%llx, block size = 0x%x\n", address, size);
}

static void hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    printf(">>> Tracing instruction at 0x%llx, instruction size = 0x%x\n", address, size);
}

static void test_arm(void)
{
    uc_engine *uc;
    uc_err err;
    uc_hook trace1, trace2;

    int r0 = 0x1234;     // R0 register
    int r2 = 0x6789;     // R1 register
    int r3 = 0x3333;     // R2 register
    int r1;     // R1 register

    printf("Emulate ARM code\n");

    // Initialize emulator in ARM mode
    err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc);
    if (err) {
        printf("Failed on uc_open() with error returned: %u (%s)\n",
            err, uc_strerror(err));
        return;
    }

    // map 2MB memory for this emulation
    uc_mem_map(uc, ADDRESS, 2 * 1024 * 1024, UC_PROT_ALL);

    // write machine code to be emulated to memory
    uc_mem_write(uc, ADDRESS, ARM_CODE, sizeof(ARM_CODE) - 1);

    // initialize machine registers
    uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    uc_reg_write(uc, UC_ARM_REG_R2, &r2);
    uc_reg_write(uc, UC_ARM_REG_R3, &r3);

    // tracing all basic blocks with customized callback
    uc_hook_add(uc, &trace1, UC_HOOK_BLOCK, hook_block, NULL, 1, 0);

    // tracing one instruction at ADDRESS with customized callback
    uc_hook_add(uc, &trace2, UC_HOOK_CODE, hook_code, NULL, ADDRESS, ADDRESS);

    // emulate machine code in infinite time (last param = 0), or when
    // finishing all the code.
    err = uc_emu_start(uc, ADDRESS, ADDRESS + sizeof(ARM_CODE) - 1, 0, 0);
    if (err) {
        printf("Failed on uc_emu_start() with error returned: %u\n", err);
    }

    // now print out some registers
    printf(">>> Emulation done. Below is the CPU context\n");

    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    printf(">>> R0 = 0x%x\n", r0);
    printf(">>> R1 = 0x%x\n", r1);

    uc_close(uc);
}

static void test_thumb(void)
{
    uc_engine *uc;
    uc_err err;
    uc_hook trace1, trace2;

    int sp = 0x1234;     // R0 register

    printf("Emulate THUMB code\n");

    // Initialize emulator in ARM mode
    err = uc_open(UC_ARCH_ARM, UC_MODE_THUMB, &uc);
    if (err) {
        printf("Failed on uc_open() with error returned: %u (%s)\n",
            err, uc_strerror(err));
        return;
    }

    // map 2MB memory for this emulation
    uc_mem_map(uc, ADDRESS, 2 * 1024 * 1024, UC_PROT_ALL);

    // write machine code to be emulated to memory
    uc_mem_write(uc, ADDRESS, THUMB_CODE, sizeof(THUMB_CODE) - 1);

    // initialize machine registers
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);

    // tracing all basic blocks with customized callback
    uc_hook_add(uc, &trace1, UC_HOOK_BLOCK, hook_block, NULL, 1, 0);

    // tracing one instruction at ADDRESS with customized callback
    uc_hook_add(uc, &trace2, UC_HOOK_CODE, hook_code, NULL, ADDRESS, ADDRESS);

    // emulate machine code in infinite time (last param = 0), or when
    // finishing all the code.
    // Note we start at ADDRESS | 1 to indicate THUMB mode.
    err = uc_emu_start(uc, ADDRESS | 1, ADDRESS + sizeof(THUMB_CODE) - 1, 0, 0);
    if (err) {
        printf("Failed on uc_emu_start() with error returned: %u\n", err);
    }

    // now print out some registers
    printf(">>> Emulation done. Below is the CPU context\n");

    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    printf(">>> SP = 0x%x\n", sp);

    uc_close(uc);
}
#endif

// from https://stackoverflow.com/a/23152590/9080566
template<class T> inline T operator~ (T a) { return (T)~(int)a; }
template<class T> inline T operator| (T a, T b) { return (T)((int)a | (int)b); }
template<class T> inline T operator& (T a, T b) { return (T)((int)a & (int)b); }
template<class T> inline T operator^ (T a, T b) { return (T)((int)a ^ (int)b); }
template<class T> inline T& operator|= (T& a, T b) { return (T&)((int&)a |= (int)b); }
template<class T> inline T& operator&= (T& a, T b) { return (T&)((int&)a &= (int)b); }
template<class T> inline T& operator^= (T& a, T b) { return (T&)((int&)a ^= (int)b); }

// from https://stackoverflow.com/a/27296/9080566
std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}

MainPage::MainPage()
{
    InitializeComponent();

    // load test Mach-O binary
    filesystem::path dir(ApplicationData::Current->TemporaryFolder->Path->Data());
    filesystem::path file("test.ipa");
    filesystem::path full = dir / file;
    unique_ptr<FatBinary> fat(Parser::parse(full.str()));
    Binary& bin = fat->at(0);

    // check header info
    auto& header = bin.header();
    if (header.cpu_type() != CPU_TYPES::CPU_TYPE_ARM) {
        throw 1;
    }

    // initialize unicorn engine
    uc_engine *uc;
    uc_err err;
    err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc);
    if (err) { // TODO: handle errors with some macro
        throw 1;
    }

    ptrdiff_t first_slide; // slide of the first segment
    bool was_first = false;
#define UPDATE_FIRST_SLIDE(val) if (!was_first) { was_first = true; first_slide = val; }

    // load segments
    for (auto& seg : bin.segments()) {
        // convert protection
        auto vmprot = seg.init_protection();
        uc_prot perms = UC_PROT_NONE;
        if (vmprot & VM_PROTECTIONS::VM_PROT_READ) {
            perms |= UC_PROT_READ;
        }
        if (vmprot & VM_PROTECTIONS::VM_PROT_WRITE) {
            perms |= UC_PROT_WRITE;
        }
        if (vmprot & VM_PROTECTIONS::VM_PROT_EXECUTE) {
            perms |= UC_PROT_EXEC;
        }

        uint64_t vaddr = seg.virtual_address();
        uint64_t vsize = seg.virtual_size();
        // TODO: virtual address and size must be 4kB-aligned for uc_mem_map_ptr to work, are they always?

        if (perms == UC_PROT_NONE) {
            // no protection means we don't have to malloc, we just map it
            uc_mem_map_ptr(uc, vaddr, vsize, perms, (void*)vaddr);

            UPDATE_FIRST_SLIDE(0)
        }
        else {
            // we allocate memory for the whole segment (which should be mapped as contiguous region of virtual memory)
            void *addr = malloc(vsize);
            auto& buff = seg.content();
            memcpy(addr, buff.data(), buff.size());
            uc_mem_map_ptr(uc, (uint64_t)addr, vsize, perms, addr);

            // set the remaining memory to zeros
            if (buff.size() < vsize) {
                memset((uint8_t*)addr + buff.size(), 0, vsize - buff.size());
            }

            ptrdiff_t slide = (uintptr_t)addr - vaddr;
            UPDATE_FIRST_SLIDE(slide)

            if (slide != 0) {
                // we have to relocate the segment
                for (auto& rel : seg.relocations()) {
                    if (rel.is_pc_relative()) {
                        throw 1;
                    }

                    if (rel.origin() == RELOCATION_ORIGINS::ORIGIN_DYLDINFO) {
                        // find base address for this relocation
                        // (inspired by ImageLoaderMachOClassic::getRelocBase)
                        if (header.has(HEADER_FLAGS::MH_SPLIT_SEGS)) {
                            throw 1;
                        }
                        uint64_t relbase = unsigned(bin.segments()[0].virtual_address()) + first_slide;

                        uint64_t reladdr = unsigned(relbase + rel.address()) + slide;
                        if (rel.size() == 32 && reladdr <= (uint64_t)addr + vsize) {
                            *(uint32_t *)(reladdr) += slide;
                        }
                        else {
                            throw 1;
                        }
                    }
#if 0
                    else if (rel.origin() == RELOCATION_ORIGINS::ORIGIN_RELOC_TABLE) {
                        switch (rel.type()) {
                        case ARM_RELOCATION::ARM_RELOC_VANILLA:
                            break;
                        }
                    }
#endif
                    else {
                        throw 1;
                    }
                }
            }
        }
    }

#undef UPDATE_FIRST_SLIDE

    // bind imported symbols
    for (auto& sym : bin.imported_symbols()) { // TODO: maybe foreach bindings?
        uint8_t symtype = sym.type();
        if (symtype & MACHO_SYMBOL_TYPES::N_STAB) {
            continue; // ignore debugging symbols
        }
        if (symtype & MACHO_SYMBOL_TYPES::N_PEXT ||
            (symtype & MACHO_SYMBOL_TYPES::N_TYPE) != N_LIST_TYPES::N_UNDF) {
            throw "unsupported symbol type";
        }
        if (symtype & MACHO_SYMBOL_TYPES::N_EXT) {
            auto& binfo = sym.binding_info();
            auto& lib = binfo.library();
            string imp = lib.name();
            string sysprefix("/System/Library/Frameworks/");
            if (imp.substr(0, sysprefix.length()) == sysprefix) {
                string fwsuffix(".framework/");
                size_t i = imp.find(fwsuffix);
                string fwname = imp.substr(i + fwsuffix.length());
                if (i != string::npos && imp.substr(sysprefix.length(), i - sysprefix.length()) == fwname) {
                    auto winlib = LoadPackagedLibrary(s2ws(fwname + ".dll").c_str(), 0);
                    if (!winlib) {
                        throw "library " + imp + " couldn't be loaded as " + fwname + ".dll";
                    }
                    string n = sym.name();
                    if (n.length() == 0 || n[0] != '_') {
                        throw 1;
                    }
                    auto addr = GetProcAddress(winlib, n.substr(1).c_str());
                    if (!addr) {
                        throw 1;
                    }
                    
                    // rewrite stub address with the found one
                    // TODO.
                }
                else {
                    throw 1;
                }
            }
            else {
                throw 1;
            }
        }
        else {
            throw "unrecognized symbol type";
        }
    }

    // load libraries
    for (auto& lib : bin.libraries()) {
        // translate name
        auto imp = lib.name();
        string exp;
        if (imp == "/System/Library/Frameworks/Foundation.framework/Foundation") exp = "Foundation.dll";
        //else throw 1;
        // TODO: map library into the Unicorn Engine
    }

    // ensure we processed all commands
    for (auto& c : bin.commands()) {
        auto type = c.command();
        switch (type) {
        case LOAD_COMMAND_TYPES::LC_SEGMENT: // segments
            break;
        case LOAD_COMMAND_TYPES::LC_DYLD_INFO:
        case LOAD_COMMAND_TYPES::LC_DYLD_INFO_ONLY: // TODO.
            break;
        default: throw 1;
        }
    }
}
