#include "codegen_internal.h"
/* ================================================================
 * Assemble and link pipeline
 * ================================================================ */
int codegen_assemble(Codegen *cg, const char *asm_file, const char *output_file) {
    /* Ensure /tmp/kernel/ exists */
    mkdir_p("/tmp/kernel");

    /* Universal binary targets: compile for multiple architectures and merge */
    if (cg->target == TARGET_UNIVERSAL || cg->target == TARGET_UNIVERSAL_ALL) {
        /* Read the generated x86_64 NASM */
        FILE *f = fopen(asm_file, "rb");
        if (!f) { fprintf(stderr, "Error: cannot read '%s'\n", asm_file); return 1; }
        fseek(f, 0, SEEK_END);
        long flen = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *src = (char *)malloc((size_t)flen + 1);
        if (!src) { fclose(f); return 1; }
        size_t rlen = fread(src, 1, (size_t)flen, f);
        src[rlen] = '\0';
        fclose(f);

        /* Parse the NASM into IR */
        AsmIRBlock block;
        if (!asm_parse_string(src, &block, asm_file)) {
            fprintf(stderr, "Error: failed to parse generated NASM\n");
            free(src);
            asm_block_free(&block);
            return 1;
        }
        free(src);

        /* Create universal builder */
        UniversalConfig config;
        universal_config_init(&config);
        if (cg->target == TARGET_UNIVERSAL_ALL) {
            config.arch_flags = (1 << ARCH_X86_64) | (1 << ARCH_ARM64) | (1 << ARCH_RISCV64);
        } else {
            config.arch_flags = (1 << ARCH_X86_64) | (1 << ARCH_ARM64);
        }
        config.verbose = 1;

        UniversalBuilder *builder = universal_builder_create(&config);
        if (!builder) { asm_block_free(&block); return 1; }

        /* Emit x86_64 NASM (passthrough) */
        {
            extern AsmBackend *asm_backend_create_x86_64(void);
            AsmBackend *backend = asm_backend_create_x86_64();
            if (!backend) { universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            size_t out_len;
            char *output = backend->emit(&block, &out_len);
            if (!output) { backend->destroy(output); universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            universal_builder_add_asm(builder, ARCH_X86_64, output, out_len);
            backend->destroy(output);
        }

        /* Emit ARM64 */
        if (config.arch_flags & (1 << ARCH_ARM64)) {
            extern AsmBackend *asm_backend_create_arm64(void);
            AsmBackend *backend = asm_backend_create_arm64();
            if (!backend) { universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            size_t out_len;
            char *output = backend->emit(&block, &out_len);
            if (!output) { backend->destroy(output); universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            universal_builder_add_asm(builder, ARCH_ARM64, output, out_len);
            backend->destroy(output);
        }

        /* Emit RISC-V */
        if (config.arch_flags & (1 << ARCH_RISCV64)) {
            extern AsmBackend *asm_backend_create_riscv64(void);
            AsmBackend *backend = asm_backend_create_riscv64();
            if (!backend) { universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            size_t out_len;
            char *output = backend->emit(&block, &out_len);
            if (!output) { backend->destroy(output); universal_builder_destroy(builder); asm_block_free(&block); return 1; }
            universal_builder_add_asm(builder, ARCH_RISCV64, output, out_len);
            backend->destroy(output);
        }

        asm_block_free(&block);

        /* Build the universal binary */
        int ret = universal_build(builder, output_file);
        universal_builder_destroy(builder);
        return ret;
    }

    /* .aelib library target: assemble to .o for archiving */
    if (cg->target == TARGET_LIB) {
        /* Always use ELF64 for .aelib — the object code is used across toolchains */
        const char *nasm_fmt = "elf64";

        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "nasm -O0 -Wno-label-redef-late -f %s -o \"%s\" \"%s\"", nasm_fmt, output_file, asm_file);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, "nasm failed (exit %d)\n", ret);
            return ret;
        }

        /* Also build the .aelib archive from the .o */
        {
            /* Read the .o file */
            FILE *f = fopen(output_file, "rb");
            if (!f) { fprintf(stderr, "Error: cannot read '%s'\n", output_file); return 1; }
            fseek(f, 0, SEEK_END);
            long flen = ftell(f);
            fseek(f, 0, SEEK_SET);
            uint8_t *code_data = (uint8_t *)malloc((size_t)flen);
            if (!code_data) { fclose(f); return 1; }
            size_t rlen = fread(code_data, 1, (size_t)flen, f);
            fclose(f);

            /* Set code data on the aelib writer */
            aelib_set_code(cg->aelib_writer, code_data, rlen);

            /* Write the .aelib file alongside the .o */
            char aelib_path[1024];
            snprintf(aelib_path, sizeof(aelib_path), "%s.aelib", output_file);
            ret = aelib_write(cg->aelib_writer, aelib_path);
            if (ret != 0) {
                fprintf(stderr, "aelib_write failed\n");
                return ret;
            }
        }

        return 0;
    }

    /* ASM listing targets: parse NASM output and re-emit through multi-target backend */
    if (cg->target == TARGET_ASM_X86_64 || cg->target == TARGET_ASM_ARM64 || cg->target == TARGET_ASM_RISCV64) {
        FILE *f = fopen(asm_file, "rb");
        if (!f) { fprintf(stderr, "Error: cannot read '%s'\n", asm_file); return 1; }
        fseek(f, 0, SEEK_END);
        long flen = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *src = (char *)malloc((size_t)flen + 1);
        if (!src) { fclose(f); return 1; }
        size_t rlen = fread(src, 1, (size_t)flen, f);
        src[rlen] = '\0';
        fclose(f);

        AsmIRBlock block;
        if (!asm_parse_string(src, &block, asm_file)) {
            fprintf(stderr, "Error: failed to parse generated NASM\n");
            free(src);
            asm_block_free(&block);
            return 1;
        }
        free(src);

        AsmBackend *backend = NULL;
        if (cg->target == TARGET_ASM_X86_64) {
            extern AsmBackend *asm_backend_create_x86_64(void);
            backend = asm_backend_create_x86_64();
        } else if (cg->target == TARGET_ASM_ARM64) {
            extern AsmBackend *asm_backend_create_arm64(void);
            backend = asm_backend_create_arm64();
        } else {
            extern AsmBackend *asm_backend_create_riscv64(void);
            backend = asm_backend_create_riscv64();
        }
        if (!backend) { fprintf(stderr, "Error: failed to create ASM backend\n"); asm_block_free(&block); return 1; }

        size_t out_len;
        char *output = backend->emit(&block, &out_len);
        if (!output) { fprintf(stderr, "Error: backend emit failed\n"); backend->destroy(output); asm_block_free(&block); return 1; }

        FILE *outf = fopen(output_file, "w");
        if (!outf) { fprintf(stderr, "Error: cannot write '%s'\n", output_file); backend->destroy(output); asm_block_free(&block); return 1; }
        fwrite(output, 1, out_len, outf);
        fclose(outf);

        backend->destroy(output);
        asm_block_free(&block);
        return 0;
    }

    /* Step 1: Determine nasm format and output object path */
    char obj_file[1024];
    const char *nasm_format;
    const char *link_cmd_prefix;
    char ld_cmd_buf[2048]; /* persistent buffer for dynamic linker cmd */

    switch (cg->target) {
        case TARGET_MACHO64:
            nasm_format = "macho64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/kernel/aether_host.o");
            link_cmd_prefix = "clang -arch x86_64 -e _aether_entry";
            break;
        case TARGET_ELF64_HOST:
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/kernel/aether_host.o");
            link_cmd_prefix = "ld -o";
            break;
        case TARGET_BOOT:
            /* Boot sector: flat binary, no linker — handled below */
            nasm_format = "bin";
            obj_file[0] = '\0';
            link_cmd_prefix = NULL;
            break;
        case TARGET_KERNEL:
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/kernel/aether_kernel.o");
            /* Use custom linker script if provided, otherwise auto-generate */
            if (cg->linker_script) {
                snprintf(ld_cmd_buf, sizeof(ld_cmd_buf), LD " -T %s -o", cg->linker_script);
                link_cmd_prefix = ld_cmd_buf;
            } else {
                FILE *ldf = fopen("/tmp/kernel/aether_ld.ld", "w");
                if (ldf) {
                    fprintf(ldf, "OUTPUT_FORMAT(elf64-x86-64)\nENTRY(_start)\nSECTIONS {\n");
                    fprintf(ldf, "  . = 0x1000000;\n");
                    fprintf(ldf, "  .text : ALIGN(16) { *(.text) *(.text.*) }\n");
                    fprintf(ldf, "  .rodata : ALIGN(16) { *(.rodata) *(.rodata.*) }\n");
                    fprintf(ldf, "  .data : ALIGN(16) { *(.data) *(.data.*) }\n");
                    fprintf(ldf, "  .bss : ALIGN(16) { *(.bss) *(.bss.*) *(COMMON) }\n");
                    fprintf(ldf, "  /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame) *(.eh_frame_hdr) }\n}\n");
                    fclose(ldf);
                }
                link_cmd_prefix = LD " -T /tmp/kernel/aether_ld.ld -o";
            }
            break;
        case TARGET_MODULE:
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/kernel/aether_module.o");
            link_cmd_prefix = "";
            break;
        case TARGET_BINARY:
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/kernel/aether_binary.o");
            /* Write linker script for Aether OS binary at 0x2000000 */
            {
                FILE *ldf = fopen("/tmp/kernel/aether_binary.ld", "w");
                if (ldf) {
                    uint64_t load_addr = (cg->entry_addr > 0) ? (uint64_t)cg->entry_addr : 0x2000000;
                    const char *entry_name = cg->entry_func ? cg->entry_func : "_start";
                    fprintf(ldf, "OUTPUT_FORMAT(elf64-x86-64)\nENTRY(%s)\nSECTIONS {\n", entry_name);
                    fprintf(ldf, "  . = 0x%lx;\n", (unsigned long)load_addr);
                    fprintf(ldf, "  .text : { *(.text) *(.text.*) }\n");
                    fprintf(ldf, "  .rodata : { *(.rodata) *(.rodata.*) }\n");
                    fprintf(ldf, "  .data : { *(.data) *(.data.*) }\n");
                    fprintf(ldf, "  .bss : { *(.bss) *(.bss.*) *(COMMON) }\n}\n");
                    fclose(ldf);
                }
            }
            link_cmd_prefix = LD " -T /tmp/kernel/aether_binary.ld -o";
            break;
        default:
            /* Freestanding — use external LD variable */
            nasm_format = "elf64";
            snprintf(obj_file, sizeof(obj_file), "/tmp/kernel/aether_host.o");
            /* Write linker script inline (or use user-supplied one) */
            if (cg->linker_script) {
                snprintf(ld_cmd_buf, sizeof(ld_cmd_buf), LD " -T %s -o", cg->linker_script);
                link_cmd_prefix = ld_cmd_buf;
            } else {
                FILE *ldf = fopen("/tmp/kernel/aether_ld.ld", "w");
                if (ldf) {
                    /* Use @entry address if set, otherwise default to 0x400000 */
                    uint64_t load_addr = (cg->entry_addr > 0) ? (uint64_t)cg->entry_addr : 0x400000;
                    /* Use @entry func name if set, otherwise default to _start */
                    const char *entry_name = cg->entry_func ? cg->entry_func : "_start";
                    fprintf(ldf, "OUTPUT_FORMAT(elf64-x86-64)\nENTRY(%s)\nSECTIONS {\n", entry_name);
                    fprintf(ldf, "  . = 0x%lx;\n", (unsigned long)load_addr);
                    fprintf(ldf, "  .text : { *(.text) *(.text.*) }\n");
                    fprintf(ldf, "  .rodata : { *(.rodata) *(.rodata.*) }\n");
                    fprintf(ldf, "  .data : { *(.data) *(.data.*) }\n");
                    fprintf(ldf, "  .bss : { *(.bss) *(.bss.*) *(COMMON) }\n}\n");
                    fclose(ldf);
                }
                link_cmd_prefix = LD " -T /tmp/kernel/aether_ld.ld -o";
            }
            break;
    }

    /* Step 2: Assemble with nasm */
    char cmd[4096];

    /* If @layout or TARGET_BOOT, assemble as flat binary — direct output, no linker */
    if (cg->has_layout || cg->target == TARGET_BOOT) {
        snprintf(cmd, sizeof(cmd), "nasm -O0 -Wno-label-redef-late -f bin -o %s %s", output_file, asm_file);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, "nasm (flat binary) failed (exit %d)\n", ret);
            return ret;
        }
        /* Verify binary fits within layout_max if specified */
        if (cg->layout_max > 0) {
            FILE *f = fopen(output_file, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long actual = ftell(f);
                fclose(f);
                if (actual > cg->layout_max) {
                    fprintf(stderr, "ERROR: binary size %ld exceeds @layout max %ld\n",
                        actual, (long)cg->layout_max);
                    return 1;
                }
            }
        }
        return 0;
    }

    snprintf(cmd, sizeof(cmd), "nasm -O2 -Wno-label-redef-late -f %s -o %s %s", nasm_format, obj_file, asm_file);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "nasm failed (exit %d)\n", ret);
        return ret;
    }

    /* Step 3: Link — include any imported .aelib .o files */
    /* Extract .o from each imported .aelib archive */
    char aelib_o_files[4096] = "";
    for (int i = 0; i < cg->aelib_import_count; i++) {
        char o_path[256];
        snprintf(o_path, sizeof(o_path), "/tmp/kernel/aelib_import_%d.o", i);
        if (aelib_extract_object(cg->aelib_imports[i], o_path) == 0) {
            size_t cur = strlen(aelib_o_files);
            snprintf(aelib_o_files + cur, sizeof(aelib_o_files) - cur, " %s", o_path);
        }
    }

    if (cg->target == TARGET_MACHO64) {
        snprintf(cmd, sizeof(cmd), "%s -o %s %s %s " SEGFAULT_HELPER, link_cmd_prefix, output_file, obj_file, aelib_o_files);
    } else if (cg->target == TARGET_ELF64_HOST) {
        snprintf(cmd, sizeof(cmd), "%s %s %s %s " SEGFAULT_HELPER, link_cmd_prefix, output_file, obj_file, aelib_o_files);
    } else if (link_cmd_prefix && link_cmd_prefix[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "%s %s %s %s", link_cmd_prefix, output_file, obj_file, aelib_o_files);
    } else {
        /* No linker step needed (module targets) */
        /* Just copy object to output */
        snprintf(cmd, sizeof(cmd), "cp %s %s", obj_file, output_file);
    }
    ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "linker failed (exit %d)\n", ret);
        return ret;
    }

    /* Cleanup object file and linker script */
    remove(obj_file);
    remove("/tmp/kernel/aether_ld.ld");
    return 0;
}