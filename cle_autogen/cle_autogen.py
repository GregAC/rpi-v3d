#!/usr/bin/python

import sys
import collections
from datetime import datetime

CLInstr = collections.namedtuple('CLInstr', ['name', 'opcode', 'rendering', 'binning', 'arguments'])

v3d_cl_instrs = [
    CLInstr('HALT'                     , 0  , True , True , []),
    CLInstr('NOP'                      , 1  , True , True , []),
    CLInstr('FLUSH'                    , 4  , False, True , []),
    CLInstr('FLUSH_ALL_STATE'          , 5  , False, True , []),
    CLInstr('START_TILE_BINNING'       , 6  , False, True , []),
    CLInstr('INCR_SEMAPHORE'           , 7  , True , True , []),
    CLInstr('WAIT_SEMAPHORE'           , 8  , True , True , []),
    CLInstr('BRANCH'                   , 16 , True , True , [
        ('branch_addr', 32)
    ]),
    CLInstr('BRANCH_SUB'               , 17 , True , True , [
        ('branch_addr',32)
    ]),
    CLInstr('RETURN'                   , 18 , True , True , []),
    CLInstr('STORE_SUBSAMPLE'          , 24 , True , False, []),
    CLInstr('STORE_SUBSAMPLE_EOF'      , 25 , True , False, []),
    CLInstr('STORE_FULL'               , 26 , True , False, [
        ('disable_colour_write', 1),
        ('disable_z_write', 1),
        ('disable_clear_on_write', 1),
        ('last_tile', 1),
        ('tile_addr', 28)
        ]),
    CLInstr('LOAD_FULL'                , 27 , True , False, [
        ('disable_colour_read', 1),
        ('disable_z_read', 1),
        ('UNUSED', 2),
        ('tile_addr', 28)
        ]),
    CLInstr('STORE_GENERAL'            , 28 , True , False, [
        ('buffer', 3),
        ('UNUSED0', 1),
        ('format', 2),
        ('mode', 2),
        ('pixel_colour_format', 2),
        ('UNUSED1', 2),
        ('disable_double_buf_swap', 1),
        ('disable_colour_clear', 1),
        ('disable_z_clear', 1),
        ('disable_vg_clear', 1),
        ('disable_colour_dump', 1),
        ('disable_z_dump', 1),
        ('disable_vg_dump', 1),
        ('last_tile', 1),
        ('frame_addr', 28)
        ]),
    CLInstr('LOAD_GENERAL'             , 29 , True , False, [
        ('buffer', 3),
        ('UNUSED0', 1),
        ('format', 2),
        ('UNUSED1', 2),
        ('pixel_colour_format', 2),
        ('UNUSED2', 2),
        ('disable_colour_load', 1),
        ('disable_z_load', 1),
        ('disable_vg_load', 1),
        ('UNUSED3', 1),
        ('frame_addr', 28)
        ]),
    CLInstr('INDEXED_PRIM_LIST'        , 32 , True , True , [
        ('prim_mode', 4),
        ('index_type', 4),
        ('length', 32),
        ('indices_addr', 32),
        ('maximum_index', 32)
        ]),
    CLInstr('VERTEX_PRIM_LIST'         , 33 , True , True , [
        ('prim_mode', 8),
        ('length', 32),
        ('vertices_addr', 32)
        ]),
    CLInstr('VG_COORD_LIST'            , 41 , True , True , [
        ('prim_mode', 4),
        ('continuation_list', 4),
        ('length', 32),
        ('coord_addr', 32)
        ]),
    CLInstr('VG_INLINE_LIST'           , 42 , True, True , [
        ('prim_mode', 4),
        ('continuation_list', 4),
        ('coord_list_BROKEN', 32)
        ]),
    CLInstr('COMPRESSED_PRIM_LIST'     , 48 , True , False, [
        ('BROKEN', 8)
        ]),
    CLInstr('CLIPPED_PRIM'             , 49 , True , False, [
        ('clip_flags', 3),
        ('clip_addr_addr', 29),
        ('BROKEN', 8)
        ]),
    CLInstr('PRIMITIVE_LIST_FORMAT'    , 56 , True , False, [
        ('prim_type', 4),
        ('data_type', 4)
        ]),
    CLInstr('GL_SHADER'                , 64 , True , True , [
        ('num_attr_arrays', 3),
        ('extended_record', 1),
        ('shader_record_addr', 28)
        ]),
    CLInstr('NV_SHADER'                , 65 , True , True , [
        ('shader_record_addr', 32)
        ]),
    CLInstr('VG_SHADER'                , 66 , True , True , [
        ('shader_record_addr', 32)
        ]),
    CLInstr('INLINE_VG_SHADER'         , 67 , True , True , [
        ('threading', 3),
        ('fragment_shader_code_addr', 29),
        ('fragment_shader_uniforms_addr', 32)
        ]),
    CLInstr('STATE_CFG'                , 96 , True , True , [
        ('enable_forward_face', 1),
        ('enable_rear_face', 1),
        ('clockwise_prims', 1),
        ('enable_depth_offset', 1),
        ('aa_lines', 1),
        ('cov_read_type', 1),
        ('rast_oversample_mode', 2),
        ('cov_pipe_select', 1),
        ('cov_update_mode', 2),
        ('cov_read_mode', 1),
        ('depth_test_func', 3),
        ('z_update_enable', 1),
        ('early_z_enable', 1),
        ('early_z_update_enable', 1),
        ('UNUSED', 6)
        ]),
    CLInstr('STATE_FLATSHADE'          , 97 , True , True , [
        ('flatshade_flags', 32)
        ]),
    CLInstr('STATE_POINT_SIZE'         , 98 , True , True , [
        ('point_size', 32)
        ]),
    CLInstr('STATE_LINE_WIDTH'         , 99 , True , True , [
        ('line_width', 32)
        ]),
    CLInstr('STATE_RHTX'               , 100, True , True , [
        ('rht_primtive_x', 16)
        ]),
    CLInstr('STATE_DEPTH_OFFSET'       , 101, True , True , [
        ('depth_offset_factor', 16),
        ('depth_offset_units', 16)
        ]),
    CLInstr('STATE_CLIP_WINDOW'        , 102, True , True , [
        ('left', 16),
        ('bottom', 16),
        ('width', 16),
        ('height', 16),
        ]),
    CLInstr('STATE_VIEWPORT_OFFSET'    , 103, True , True , [
        ('viewport_x', 16),
        ('viewport_y', 16)
        ]),
    CLInstr('STATE_CLIPZ'              , 104, True , True , [
        ('min_z', 32),
        ('max_z', 32)
        ]),
    CLInstr('STATE_CLIPPER_XY'         , 105, False, True , [
        ('viewport_half_width', 32),
        ('viewport_half_height', 32)
        ]),
    CLInstr('STATE_CLIPPER_Z'          , 106, False, True , [
        ('viewport_z_scale', 32),
        ('viewport_z_offset', 32)
        ]),
    CLInstr('STATE_TILE_BINNING_MODE'  , 112, False, True , [
        ('tile_mem_addr', 32),
        ('tile_mem_size', 32),
        ('tile_state_addr', 32),
        ('w_in_tiles', 8),
        ('h_in_tiles', 8),
        ('multisample', 1),
        ('colour_64', 1),
        ('auto_init_tile_state', 1),
        ('tile_initial_block_size', 2),
        ('tile_block_size', 2),
        ('double_buffer', 1)
        ]),
    CLInstr('STATE_TILE_RENDERING_MODE', 113, True , True , [
        ('framebuffer_address', 32),
        ('width', 16),
        ('height', 16),
        ('multisample', 1),
        ('colour_64', 1),
        ('colour_format', 2),
        ('decimate_mode', 2),
        ('memory_format', 2),
        ('enable_vg_mask', 1),
        ('coverage_mode', 1),
        ('early_z_update_dir', 1),
        ('early_z_disable', 1),
        ('double_buffer', 1),
        ('UNUSED', 3)
        ]),
    CLInstr('STATE_CLEARCOL'           , 114, True , True , [
        ('clear_colour0', 32),
        ('clear_colour1', 32),
        ('clear_z', 24),
        ('clear_vg_mask', 8),
        ('clear_stencil', 8)
        ]),
    CLInstr('STATE_TILE_COORDS'        , 115, True , True , [
        ('column', 8),
        ('row', 8)
        ]),
]

#Not really a CL instruction, however similar structure so we create a fake
#CLInstr so we can autogenerate code for dealing with them.
shader_record = CLInstr('SHADER_RECORD', -1, True, True, [
      ('flags', 16),
      ('fs_num_uniforms', 8),
      ('fs_num_varyings', 8),
      ('fs_code_addr', 32),
      ('fs_uniforms_addr', 32),
      ('vs_num_uniforms', 16),
      ('vs_attr_array_select', 8),
      ('vs_total_attr_size', 8),
      ('vs_code_addr', 32),
      ('vs_uniforms_addr', 32),
      ('cs_num_uniforms', 16),
      ('cs_attr_array_select', 8),
      ('cs_total_attr_size', 8),
      ('cs_code_addr', 32),
      ('cs_uniforms_addr', 32)
   ])

attr_array_record = CLInstr('ATTR_ARRAY_RECORD', -1, True, True, [
      ('array_base_addr', 32),
      ('array_size_bytes', 8),
      ('array_stride', 8),
      ('array_vs_vpm_offset', 8),
      ('array_cs_vpm_offset', 8)
      ])

def write_out_instr_defs(instrs, out_file):
    for instr in instrs:
        out_file.write('#define V3D_HW_INSTR_{0} {1}\n'.format(instr.name, instr.opcode))

    out_file.write('\n')

def choose_c_type(arg):
    if arg[1] <= 8:
        return 'uint8_t'
    elif arg[1] <= 16:
        return 'uint16_t'
    elif arg[1] <= 32:
        return 'uint32_t'
    else:
        return 'uint64_t'

def write_out_instr_struct(instr, out_file):
    if(instr.opcode == -1):
       out_file.write('''typedef struct {\n''')
    else:
      out_file.write('''typedef struct {\n\tuint8_t opcode;\n''')

    #TODO: Check that the c bit packing won't leave any gaps
    for a in instr.arguments:
        out_file.write('\t{0} {1} : {2};\n'.format(choose_c_type(a), a[0], a[1]))

    out_file.write('}} __attribute__((packed)) instr_{0}_t ;\n\n'.format(instr.name))

def write_out_instr_emit_fun(instr, out_file):
    out_file.write('void emit_{0}(void** cur_ins'.format(instr.name))
    for a in instr.arguments:
        out_file.write(', {0} {1}'.format(choose_c_type(a), a[0]))

    out_file.write(''') {{
\tinstr_{instr_name}_t  new_ins;
\tinstr_{instr_name}_t* ins = (instr_{instr_name}_t*)*cur_ins;
\t
'''.format(instr_name = instr.name))
    if(instr.opcode != -1):
      out_file.write('\tnew_ins.opcode = {0};\n'.format(instr.opcode))

    for a in instr.arguments:
        out_file.write('\tnew_ins.{0} = {0};\n'.format(a[0]))

    out_file.write('''\n\t*ins = new_ins;
\t*cur_ins = ins + 1;
}\n\n''')

def write_out_calc_next_ins_fun(instrs, out_file):
    out_file.write('''void* calc_next_ins(void* cur_ins) {
\tuint8_t* opcode = cur_ins;
\t
\tswitch(*opcode) {
''')

    for instr in instrs:
        out_file.write('\t\tcase V3D_HW_INSTR_{0}: return cur_ins + sizeof(instr_{0}_t);\n'.format(instr.name))

    out_file.write('''\t\tdefault: return 0;
\t}
\t
\treturn 0; //Should never get here
}\n\n''')

def write_out_instr_disassemble_fun(instr, out_file):
    out_file.write('''int disassemble_{0}(instr_{0}_t* ins, FILE* out) {{
\tfprintf(out, "{0}\\n");
'''.format(instr.name))

    for a in instr.arguments:
        out_file.write('\tfprintf(out, "\\t{0}: %x\\n", (uint32_t)ins->{0});\n'.format(a[0]))

    out_file.write('}\n\n')

def write_out_disassemble_fun(instrs, out_file):
    out_file.write('''int disassemble_instr(void* cur_ins, FILE* out) {
\tuint8_t* opcode = cur_ins;
\t
\tswitch(*opcode) {
''')

    for instr in instrs:
        out_file.write('\t\tcase V3D_HW_INSTR_{0}: disassemble_{0}((instr_{0}_t*)cur_ins, out); return 0;\n'.format(instr.name))

    out_file.write('''\t\tdefault: return 1;
\t}
\t
\treturn 1; //Should never get here
}\n\n''')

def write_out_instr_fun_defs(instr, out_file):
    out_file.write('void emit_{0}(void** cur_ins'.format(instr.name))
    for a in instr.arguments:
        out_file.write(', {0} {1}'.format(choose_c_type(a), a[0]))
    out_file.write(');\n')

    out_file.write('int disassemble_{0}(instr_{0}_t* ins, FILE* out);\n'.format(instr.name))

    out_file.write('\n')

h_header = '''//Auto-generated code for construction and disassembly of V3D CLE control lists
//Generated on {0}

#ifndef __V3D_CL_INSTR_AUTOGEN_H__
#define __V3D_CL_INSTR_AUTOGEN_H__

#include <stdio.h>
#include <stdint.h>

'''.format(datetime.now().strftime('%d/%m/%Y %H:%M'))

h_footer = '''void* calc_next_ins(void* cur_ins);
int disassemble_instr(void* cur_ins, FILE* out);

#endif

'''

c_header = '''#include <stdio.h>
#include "{header_filename}"

'''

def main(out_filename):
   c_filename = out_filename + '.c'
   c_out_file = open(c_filename, 'w')

   h_filename = out_filename + '.h'
   h_out_file = open(h_filename, 'w')

   h_out_file.write(h_header)
   write_out_instr_defs(v3d_cl_instrs, h_out_file)
   
   for instr in v3d_cl_instrs:
       write_out_instr_struct(instr, h_out_file)
       write_out_instr_fun_defs(instr, h_out_file)

   write_out_instr_struct(shader_record, h_out_file)
   write_out_instr_fun_defs(shader_record, h_out_file)
   write_out_instr_struct(attr_array_record, h_out_file)
   write_out_instr_fun_defs(attr_array_record, h_out_file)
   
   h_out_file.write(h_footer)
   
   c_out_file.write(c_header.format(header_filename=h_filename))
   for instr in v3d_cl_instrs:
       write_out_instr_emit_fun(instr, c_out_file)
       write_out_instr_disassemble_fun(instr, c_out_file)

   write_out_instr_emit_fun(shader_record, c_out_file)
   write_out_instr_disassemble_fun(shader_record, c_out_file)
   write_out_instr_emit_fun(attr_array_record, c_out_file)
   write_out_instr_disassemble_fun(attr_array_record, c_out_file)

   write_out_calc_next_ins_fun(v3d_cl_instrs, c_out_file)
   write_out_disassemble_fun(v3d_cl_instrs, c_out_file)

   c_out_file.close()
   h_out_file.close()

if __name__ == '__main__':
    if(len(sys.argv) != 2):
        print 'Usage %s out_name' % sys.argv[0]
    else:
        main(sys.argv[1])

