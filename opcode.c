/* OPCODE.C     (c) Copyright Jan Jaeger, 2000-2002                  */
/*              Instruction decoding functions                       */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2002      */

/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2002      */


#include "feature.h"


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE3)
 #define  _GEN_ARCH _ARCHMODE3
 #include "opcode.c"
 #undef   _GEN_ARCH
#endif

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "opcode.c"
 #undef   _GEN_ARCH
#endif

#endif /*!defined(_GEN_ARCH)*/


#include "hercules.h"

#include "opcode.h"


#define UNDEF_INST(_x) \
        DEF_INST(_x) { ARCH_DEP(operation_exception) \
        (inst,execflag,regs); }


#if !defined(FEATURE_CHANNEL_SUBSYSTEM)
 UNDEF_INST(clear_subchannel)
 UNDEF_INST(halt_subchannel)
 UNDEF_INST(modify_subchannel)
 UNDEF_INST(resume_subchannel)
 UNDEF_INST(set_address_limit)
 UNDEF_INST(set_channel_monitor)
 UNDEF_INST(start_subchannel)
 UNDEF_INST(store_channel_path_status)
 UNDEF_INST(store_channel_report_word)
 UNDEF_INST(store_subchannel)
 UNDEF_INST(test_pending_interruption)
 UNDEF_INST(test_subchannel)
#endif /*!defined(FEATURE_CHANNEL_SUBSYSTEM)*/


#if !defined(FEATURE_S370_CHANNEL)
 UNDEF_INST(start_io)
 UNDEF_INST(test_io)
 UNDEF_INST(halt_io)
 UNDEF_INST(test_channel)
 UNDEF_INST(store_channel_id)
#endif /*!defined(FEATURE_S370_CHANNEL)*/


#if !defined(FEATURE_IMMEDIATE_AND_RELATIVE)
 UNDEF_INST(test_under_mask_high)
 UNDEF_INST(test_under_mask_low)
 UNDEF_INST(branch_relative_on_condition)
 UNDEF_INST(branch_relative_and_save)
 UNDEF_INST(branch_relative_on_count)
 UNDEF_INST(load_halfword_immediate)
 UNDEF_INST(add_halfword_immediate)
 UNDEF_INST(multiply_halfword_immediate)
 UNDEF_INST(compare_halfword_immediate)
 UNDEF_INST(multiply_single_register)
 UNDEF_INST(multiply_single)
 UNDEF_INST(branch_relative_on_index_high)
 UNDEF_INST(branch_relative_on_index_low_or_equal)
#endif /*!defined(FEATURE_IMMEDIATE_AND_RELATIVE)*/


#if !defined(FEATURE_COMPARE_AND_MOVE_EXTENDED)
 UNDEF_INST(compare_logical_long_extended)
 UNDEF_INST(move_long_extended)
#endif /*!defined(FEATURE_COMPARE_AND_MOVE_EXTENDED)*/


#if !defined(FEATURE_CHECKSUM_INSTRUCTION)
 UNDEF_INST(checksum)
#endif /*!defined(FEATURE_CHECKSUM_INSTRUCTION)*/


#if !defined(FEATURE_PERFORM_LOCKED_OPERATION)
 UNDEF_INST(perform_locked_operation)
#endif /*!defined(FEATURE_PERFORM_LOCKED_OPERATION)*/


#if !defined(FEATURE_SUBSPACE_GROUP)
 UNDEF_INST(branch_in_subspace_group)
#endif /*!defined(FEATURE_SUBSPACE_GROUP)*/


#if !defined(FEATURE_BRANCH_AND_SET_AUTHORITY)
 UNDEF_INST(branch_and_set_authority)
#endif /*!defined(FEATURE_BRANCH_AND_SET_AUTHORITY)*/


#if !defined(FEATURE_EXPANDED_STORAGE)
 UNDEF_INST(page_in)
 UNDEF_INST(page_out)
#endif /*!defined(FEATURE_EXPANDED_STORAGE)*/


#if !defined(FEATURE_BROADCASTED_PURGING)
 UNDEF_INST(compare_and_swap_and_purge)
#endif /*!defined(FEATURE_BROADCASTED_PURGING)*/


#if !defined(FEATURE_BIMODAL_ADDRESSING)
 UNDEF_INST(branch_and_set_mode)
 UNDEF_INST(branch_and_save_and_set_mode)
#endif /*!defined(FEATURE_BIMODAL_ADDRESSING)*/


#if !defined(FEATURE_MOVE_PAGE_FACILITY_2)
 UNDEF_INST(move_page)
 UNDEF_INST(invalidate_expanded_storage_block_entry)
#endif /*!defined(FEATURE_MOVE_PAGE_FACILITY_2)*/


#if !defined(FEATURE_BASIC_STORAGE_KEYS)
 UNDEF_INST(insert_storage_key)
 UNDEF_INST(set_storage_key)
 UNDEF_INST(reset_reference_bit)
#endif /*!defined(FEATURE_BASIC_STORAGE_KEYS)*/


#if !defined(FEATURE_LINKAGE_STACK)
 UNDEF_INST(branch_and_stack)
 UNDEF_INST(modify_stacked_state)
 UNDEF_INST(extract_stacked_registers)
 UNDEF_INST(extract_stacked_state)
 UNDEF_INST(program_return)
 UNDEF_INST(trap2)
 UNDEF_INST(trap4)
#endif /*!defined(FEATURE_LINKAGE_STACK)*/


#if !defined(FEATURE_DUAL_ADDRESS_SPACE)
 UNDEF_INST(insert_address_space_control)
 UNDEF_INST(set_secondary_asn)
 UNDEF_INST(extract_primary_asn)
 UNDEF_INST(extract_secondary_asn)
 UNDEF_INST(program_call)
 UNDEF_INST(program_transfer)
 UNDEF_INST(set_address_space_control_x)
 UNDEF_INST(load_address_space_parameters)
#endif /*!defined(FEATURE_DUAL_ADDRESS_SPACE)*/


#if !defined(FEATURE_ACCESS_REGISTERS)
 UNDEF_INST(load_access_multiple)
 UNDEF_INST(store_access_multiple)
 UNDEF_INST(purge_accesslist_lookaside_buffer)
 UNDEF_INST(test_access)
 UNDEF_INST(copy_access)
 UNDEF_INST(set_access_register)
 UNDEF_INST(extract_access_register)
#endif /*!defined(FEATURE_ACCESS_REGISTERS)*/


#if !defined(FEATURE_EXTENDED_STORAGE_KEYS)
 UNDEF_INST(insert_storage_key_extended)
 UNDEF_INST(reset_reference_bit_extended)
 UNDEF_INST(set_storage_key_extended)
#endif /*!defined(FEATURE_EXTENDED_STORAGE_KEYS)*/

#if !defined(FEATURE_TEST_BLOCK)
 UNDEF_INST(test_block)
#endif /*!defined(FEATURE_TEST_BLOCK)*/


#if !defined(FEATURE_EXTENDED_TOD_CLOCK)
 UNDEF_INST(set_clock_programmable_field)
 UNDEF_INST(store_clock_extended)
#endif /*!defined(FEATURE_EXTENDED_TOD_CLOCK)*/


#if !defined(FEATURE_STORE_SYSTEM_INFORMATION)
 UNDEF_INST(store_system_information)
#endif /*!defined(FEATURE_STORE_SYSTEM_INFORMATION)*/


#if !defined(FEATURE_VECTOR_FACILITY)
 UNDEF_INST(execute_a4xx)
 #if !defined(FEATURE_ESAME) && !defined(FEATURE_ESAME_N3_ESA390)
  UNDEF_INST(execute_a5xx)
 #endif /*!defined(FEATURE_ESAME)*/

 UNDEF_INST(execute_a6xx)
 UNDEF_INST(execute_e4xx)

 UNDEF_INST(v_test_vmr)
 UNDEF_INST(v_complement_vmr)
 UNDEF_INST(v_count_left_zeros_in_vmr)
 UNDEF_INST(v_count_ones_in_vmr)
 UNDEF_INST(v_extract_vct)
 UNDEF_INST(v_extract_vector_modes)
 UNDEF_INST(v_restore_vr)
 UNDEF_INST(v_save_changed_vr)
 UNDEF_INST(v_save_vr)
 UNDEF_INST(v_load_vmr)
 UNDEF_INST(v_load_vmr_complement)
 UNDEF_INST(v_store_vmr)
 UNDEF_INST(v_and_to_vmr)
 UNDEF_INST(v_or_to_vmr)
 UNDEF_INST(v_exclusive_or_to_vmr)
 UNDEF_INST(v_save_vsr)
 UNDEF_INST(v_save_vmr)
 UNDEF_INST(v_restore_vsr)
 UNDEF_INST(v_restore_vmr)
 UNDEF_INST(v_load_vct_from_address)
 UNDEF_INST(v_clear_vr)
 UNDEF_INST(v_set_vector_mask_mode)
 UNDEF_INST(v_load_vix_from_address)
 UNDEF_INST(v_store_vector_parameters)
 UNDEF_INST(v_save_vac)
 UNDEF_INST(v_restore_vac)
#endif /*!defined(FEATURE_VECTOR_FACILITY)*/


#if !defined(FEATURE_ESAME) && !defined(FEATURE_ESAME_N3_ESA390)
 UNDEF_INST(execute_b9xx)
 UNDEF_INST(execute_e3xx)
 UNDEF_INST(execute_ebxx)
 UNDEF_INST(execute_ecxx)
 UNDEF_INST(execute_c0xx)
 UNDEF_INST(set_address_mode_24)
 UNDEF_INST(set_address_mode_31)
 UNDEF_INST(set_address_mode_64)
 UNDEF_INST(test_under_mask_highword_high)
 UNDEF_INST(test_under_mask_highword_low)
 UNDEF_INST(branch_relative_on_count_long)
 UNDEF_INST(load_long_halfword_immediate)
 UNDEF_INST(add_long_halfword_immediate)
 UNDEF_INST(multiply_long_halfword_immediate)
 UNDEF_INST(compare_long_halfword_immedate)
 UNDEF_INST(load_psw_extended)
#endif /*!defined(FEATURE_ESAME)*/


#if !defined(FEATURE_BASIC_FP_EXTENSIONS)
 UNDEF_INST(execute_b3xx)
 UNDEF_INST(execute_edxx)
#endif /*!defined(FEATURE_BASIC_FP_EXTENSIONS)*/


#if !defined(FEATURE_HEXADECIMAL_FLOATING_POINT)
 UNDEF_INST(load_positive_float_long_reg)
 UNDEF_INST(load_negative_float_long_reg)
 UNDEF_INST(load_and_test_float_long_reg)
 UNDEF_INST(load_complement_float_long_reg)
 UNDEF_INST(halve_float_long_reg)
 UNDEF_INST(round_float_long_reg)
 UNDEF_INST(multiply_float_ext_reg)
 UNDEF_INST(multiply_float_long_to_ext_reg)
 UNDEF_INST(load_float_long_reg)
 UNDEF_INST(compare_float_long_reg)
 UNDEF_INST(add_float_long_reg)
 UNDEF_INST(subtract_float_long_reg)
 UNDEF_INST(multiply_float_long_reg)
 UNDEF_INST(divide_float_long_reg)
 UNDEF_INST(add_unnormal_float_long_reg)
 UNDEF_INST(subtract_unnormal_float_long_reg)
 UNDEF_INST(load_positive_float_short_reg)
 UNDEF_INST(load_negative_float_short_reg)
 UNDEF_INST(load_and_test_float_short_reg)
 UNDEF_INST(load_complement_float_short_reg)
 UNDEF_INST(halve_float_short_reg)
 UNDEF_INST(round_float_short_reg)
 UNDEF_INST(add_float_ext_reg)
 UNDEF_INST(subtract_float_ext_reg)
 UNDEF_INST(load_float_short_reg)
 UNDEF_INST(compare_float_short_reg)
 UNDEF_INST(add_float_short_reg)
 UNDEF_INST(subtract_float_short_reg)
 UNDEF_INST(multiply_float_short_to_long_reg)
 UNDEF_INST(divide_float_short_reg)
 UNDEF_INST(add_unnormal_float_short_reg)
 UNDEF_INST(subtract_unnormal_float_short_reg)
 UNDEF_INST(store_float_long)
 UNDEF_INST(multiply_float_long_to_ext)
 UNDEF_INST(load_float_long)
 UNDEF_INST(compare_float_long)
 UNDEF_INST(add_float_long)
 UNDEF_INST(subtract_float_long)
 UNDEF_INST(multiply_float_long)
 UNDEF_INST(divide_float_long)
 UNDEF_INST(add_unnormal_float_long)
 UNDEF_INST(subtract_unnormal_float_long)
 UNDEF_INST(store_float_short)
 UNDEF_INST(load_float_short)
 UNDEF_INST(compare_float_short)
 UNDEF_INST(add_float_short)
 UNDEF_INST(subtract_float_short)
 UNDEF_INST(multiply_float_short_to_long)
 UNDEF_INST(divide_float_short)
 UNDEF_INST(add_unnormal_float_short)
 UNDEF_INST(subtract_unnormal_float_short)
 UNDEF_INST(divide_float_ext_reg)
#endif /*!defined(FEATURE_HEXADECIMAL_FLOATING_POINT)*/


#if !defined(FEATURE_HFP_EXTENSIONS)
 UNDEF_INST(loadlength_float_short_to_long_reg)
 UNDEF_INST(loadlength_float_long_to_ext_reg)
 UNDEF_INST(loadlength_float_short_to_ext_reg)
 UNDEF_INST(squareroot_float_ext_reg)
 UNDEF_INST(multiply_float_short_reg)
 UNDEF_INST(load_positive_float_ext_reg)
 UNDEF_INST(load_negative_float_ext_reg)
 UNDEF_INST(load_and_test_float_ext_reg)
 UNDEF_INST(load_complement_float_ext_reg)
 UNDEF_INST(round_float_ext_to_short_reg)
 UNDEF_INST(load_fp_int_float_ext_reg)
 UNDEF_INST(compare_float_ext_reg)
 UNDEF_INST(load_fp_int_float_short_reg)
 UNDEF_INST(load_fp_int_float_long_reg)
 UNDEF_INST(convert_fixed_to_float_short_reg)
 UNDEF_INST(convert_fixed_to_float_long_reg)
 UNDEF_INST(convert_fixed_to_float_ext_reg)
 UNDEF_INST(convert_float_short_to_fixed_reg)
 UNDEF_INST(convert_float_long_to_fixed_reg)
 UNDEF_INST(convert_float_ext_to_fixed_reg)
 UNDEF_INST(loadlength_float_short_to_long)
 UNDEF_INST(loadlength_float_long_to_ext)
 UNDEF_INST(loadlength_float_short_to_ext)
 UNDEF_INST(squareroot_float_short)
 UNDEF_INST(squareroot_float_long)
 UNDEF_INST(multiply_float_short)
#endif /*!defined(FEATURE_HFP_EXTENSIONS)*/


#if !defined(FEATURE_FPS_EXTENSIONS)
 UNDEF_INST(convert_bfp_long_to_float_long_reg)
 UNDEF_INST(convert_bfp_short_to_float_long_reg)
 UNDEF_INST(convert_float_long_to_bfp_long_reg)
 UNDEF_INST(convert_float_long_to_bfp_short_reg)
 UNDEF_INST(load_float_ext_reg)
 UNDEF_INST(load_zero_float_ext_reg)
 UNDEF_INST(load_zero_float_long_reg)
 UNDEF_INST(load_zero_float_short_reg)
#endif /*!defined(FEATURE_FPS_EXTENSIONS)*/


#if !defined(FEATURE_BINARY_FLOATING_POINT)
 UNDEF_INST(store_fpc)
 UNDEF_INST(load_fpc)
 UNDEF_INST(set_fpc)
 UNDEF_INST(extract_fpc)
 UNDEF_INST(set_rounding_mode)
#endif /*!defined(FEATURE_BINARY_FLOATING_POINT)*/


#if !defined(FEATURE_BINARY_FLOATING_POINT) || defined(NO_IEEE_SUPPORT)
 UNDEF_INST(add_bfp_ext_reg)
 UNDEF_INST(add_bfp_long)
 UNDEF_INST(add_bfp_long_reg)
 UNDEF_INST(add_bfp_short)
 UNDEF_INST(add_bfp_short_reg)
 UNDEF_INST(compare_and_signal_bfp_ext_reg)
 UNDEF_INST(compare_and_signal_bfp_long)
 UNDEF_INST(compare_and_signal_bfp_long_reg)
 UNDEF_INST(compare_and_signal_bfp_short)
 UNDEF_INST(compare_and_signal_bfp_short_reg)
 UNDEF_INST(compare_bfp_ext_reg)
 UNDEF_INST(compare_bfp_long)
 UNDEF_INST(compare_bfp_long_reg)
 UNDEF_INST(compare_bfp_short)
 UNDEF_INST(compare_bfp_short_reg)
 UNDEF_INST(convert_bfp_long_to_fix32_reg)
 UNDEF_INST(convert_bfp_short_to_fix32_reg)
 UNDEF_INST(convert_fix32_to_bfp_long_reg)
 UNDEF_INST(convert_fix32_to_bfp_short_reg)
 UNDEF_INST(convert_fix64_to_bfp_long_reg);
 UNDEF_INST(convert_fix64_to_bfp_short_reg);
 UNDEF_INST(convert_bfp_long_to_fix64_reg);
 UNDEF_INST(convert_bfp_short_to_fix64_reg);                    
 UNDEF_INST(divide_bfp_ext_reg)
 UNDEF_INST(divide_bfp_long)
 UNDEF_INST(divide_bfp_long_reg)
 UNDEF_INST(divide_bfp_short)
 UNDEF_INST(divide_bfp_short_reg)
 UNDEF_INST(load_and_test_bfp_ext_reg)
 UNDEF_INST(load_and_test_bfp_long_reg)
 UNDEF_INST(load_and_test_bfp_short_reg)
 UNDEF_INST(load_complement_bfp_ext_reg)
 UNDEF_INST(load_complement_bfp_long_reg)
 UNDEF_INST(load_complement_bfp_short_reg)
 UNDEF_INST(load_negative_bfp_ext_reg)
 UNDEF_INST(load_negative_bfp_long_reg)
 UNDEF_INST(load_negative_bfp_short_reg)
 UNDEF_INST(load_positive_bfp_ext_reg)
 UNDEF_INST(load_positive_bfp_long_reg)
 UNDEF_INST(load_positive_bfp_short_reg)
 UNDEF_INST(loadlength_bfp_short_to_long)
 UNDEF_INST(loadlength_bfp_short_to_long_reg)
 UNDEF_INST(multiply_bfp_ext_reg)
 UNDEF_INST(multiply_bfp_long)
 UNDEF_INST(multiply_bfp_long_reg)
 UNDEF_INST(multiply_bfp_short)
 UNDEF_INST(multiply_bfp_short_reg)
 UNDEF_INST(round_bfp_long_to_short_reg)
 UNDEF_INST(squareroot_bfp_ext_reg)
 UNDEF_INST(squareroot_bfp_long)
 UNDEF_INST(squareroot_bfp_long_reg)
 UNDEF_INST(squareroot_bfp_short)
 UNDEF_INST(squareroot_bfp_short_reg)
 UNDEF_INST(subtract_bfp_ext_reg)
 UNDEF_INST(subtract_bfp_long)
 UNDEF_INST(subtract_bfp_long_reg)
 UNDEF_INST(subtract_bfp_short)
 UNDEF_INST(subtract_bfp_short_reg)
 UNDEF_INST(testdataclass_bfp_short)
 UNDEF_INST(testdataclass_bfp_long)
 UNDEF_INST(testdataclass_bfp_ext)
#endif /*!defined(FEATURE_BINARY_FLOATING_POINT)*/


#if !defined(FEATURE_EMULATE_VM)
 UNDEF_INST(inter_user_communication_vehicle)
#endif /*!defined(FEATURE_EMULATE_VM)*/


#if !defined(FEATURE_RESUME_PROGRAM)
 UNDEF_INST(resume_program)
#endif /*!defined(FEATURE_RESUME_PROGRAM)*/


#if !defined(FEATURE_COMPRESSION)
 UNDEF_INST(compression_call)
#endif /*!defined(FEATURE_COMPRESSION)*/


#if !defined(FEATURE_LOCK_PAGE)
 UNDEF_INST(lock_page)
#endif /*!defined(FEATURE_LOCK_PAGE)*/


#if !defined(FEATURE_SQUARE_ROOT)
 UNDEF_INST(squareroot_float_long_reg)
 UNDEF_INST(squareroot_float_short_reg)
#endif /*!defined(FEATURE_SQUARE_ROOT)*/


#if !defined(FEATURE_INTERPRETIVE_EXECUTION)
 UNDEF_INST(start_interpretive_execution)
#endif /*!defined(FEATURE_INTERPRETIVE_EXECUTION)*/


#if !defined(FEATURE_CHANNEL_SWITCHING)
 UNDEF_INST(connect_channel_set)
 UNDEF_INST(disconnect_channel_set)
#endif /*!defined(FEATURE_CHANNEL_SWITCHING)*/


#if !defined(FEATURE_EXTENDED_TRANSLATION)
 UNDEF_INST(translate_extended)
 UNDEF_INST(convert_unicode_to_utf8)
 UNDEF_INST(convert_utf8_to_unicode)
#endif /*!defined(FEATURE_EXTENDED_TRANSLATION)*/


#if !defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)
 UNDEF_INST(compare_logical_long_unicode)
 UNDEF_INST(move_long_unicode)
 UNDEF_INST(pack_ascii)
 UNDEF_INST(pack_unicode)
 UNDEF_INST(test_decimal)
 UNDEF_INST(translate_one_to_one)
 UNDEF_INST(translate_one_to_two)
 UNDEF_INST(translate_two_to_one)
 UNDEF_INST(translate_two_to_two)
 UNDEF_INST(unpack_ascii)
 UNDEF_INST(unpack_unicode)
#endif /*!defined(FEATURE_EXTENDED_TRANSLATION_FACILITY_2)*/


#if !defined(FEATURE_LOAD_REVERSED) && !defined(FEATURE_ESAME_N3_ESA390)
 UNDEF_INST(load_reversed_register)
 UNDEF_INST(load_reversed)
 UNDEF_INST(load_reversed_half)
 UNDEF_INST(store_reversed)
 UNDEF_INST(store_reversed_half)
 #if !defined(FEATURE_ESAME)
 UNDEF_INST(load_reversed_long_register)
 UNDEF_INST(store_reversed_long)
 #endif /*!defined(FEATURE_ESAME)*/
#endif /*!defined(FEATURE_LOAD_REVERSED) && !defined(FEATURE_ESAME_N3_ESA390)*/


#if !defined(FEATURE_SERVICE_PROCESSOR)
 UNDEF_INST(service_call)
#endif /*!defined(FEATURE_SERVICE_PROCESSOR)*/


#if !defined(FEATURE_CHSC)
 UNDEF_INST(channel_subsystem_call)
#endif /*!defined(FEATURE_CHSC)*/


#if !defined(FEATURE_ESAME_N3_ESA390) && !defined(FEATURE_ESAME)
 UNDEF_INST(add_logical_carry);
 UNDEF_INST(add_logical_carry_register);
 UNDEF_INST(branch_relative_and_save_long);
 UNDEF_INST(branch_relative_on_condition_long);
 UNDEF_INST(divide_logical);
 UNDEF_INST(divide_logical_register);
 UNDEF_INST(extract_psw);
 UNDEF_INST(load_address_relative_long);
 UNDEF_INST(multiply_logical);
 UNDEF_INST(multiply_logical_register);
 UNDEF_INST(rotate_left_single_logical);
 UNDEF_INST(set_addressing_mode_24);
 UNDEF_INST(set_addressing_mode_31);
 UNDEF_INST(subtract_logical_borrow);
 UNDEF_INST(subtract_logical_borrow_register);
 UNDEF_INST(test_addressing_mode);
#endif /*!defined(FEATURE_ESAME_N3_ESA390) && !defined(FEATURE_ESAME)*/


#if !defined(FEATURE_ESAME_N3_ESA390) && !defined(_900) && !defined(FEATURE_ESAME)
 UNDEF_INST(store_facilities_list);
#endif /*!defined(FEATURE_ESAME_N3_ESA390) && !defined(_900)*/


#if !defined(FEATURE_CANCEL_IO_FACILITY)
 UNDEF_INST(cancel_subchannel)
#endif /*!defined(FEATURE_CANCEL_IO_FACILITY)*/


/* The following execute_xxxx routines can be optimized by the
   compiler to an indexed jump, leaving the stack frame untouched
   as the called routine has the same arguments, and the routine
   exits immediately after the call.                             *JJ */

DEF_INST(execute_01xx)
{
    opcode_01xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_a7xx)
{
    opcode_a7xx[inst[1] & 0x0F][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_b2xx)
{
    opcode_b2xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}


#if defined(FEATURE_BASIC_FP_EXTENSIONS)
DEF_INST(execute_b3xx)
{
    opcode_b3xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}

DEF_INST(execute_edxx)
{
    opcode_edxx[inst[5]][ARCH_MODE](inst, execflag, regs);
}
#endif /*defined(FEATURE_BASIC_FP_EXTENSIONS)*/


DEF_INST(execute_e5xx)
{
    opcode_e5xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}


#if defined(FEATURE_ESAME) || defined(FEATURE_ESAME_N3_ESA390)
DEF_INST(execute_a5xx)
{
    opcode_a5xx[inst[1] & 0x0F][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_b9xx)
{
    opcode_b9xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_e3xx)
{
    opcode_e3xx[inst[5]][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_ebxx)
{
    opcode_ebxx[inst[5]][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_ecxx)
{
    opcode_ecxx[inst[5]][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_c0xx)
{
    opcode_c0xx[inst[1] & 0x0F][ARCH_MODE](inst, execflag, regs);
}
#endif /*defined(FEATURE_ESAME)*/


#if defined(FEATURE_VECTOR_FACILITY)

DEF_INST(execute_a4xx)
{
    v_opcode_a4xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_a5xx)
{
    v_opcode_a5xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_a6xx)
{
    v_opcode_a6xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}


DEF_INST(execute_e4xx)
{
    v_opcode_e4xx[inst[1]][ARCH_MODE](inst, execflag, regs);
}

#endif /*defined(FEATURE_VECTOR_FACILITY)*/


DEF_INST(operation_exception)
{
    if( !execflag )
    {
        regs->psw.ilc = (inst[0] < 0x40) ? 2 :
                        (inst[0] < 0xC0) ? 4 : 6;
        regs->psw.IA += regs->psw.ilc;
        regs->psw.IA &= ADDRESS_MAXWRAP(regs);
    }

#if defined(MODEL_DEPENDENT)
#if defined(_FEATURE_SIE)
    /* The B2XX extended opcodes which are not defined are always
       intercepted by SIE when issued in supervisor state */
    if(!regs->psw.prob && inst[0] == 0xB2)
        SIE_INTERCEPT(regs);
#endif /*defined(_FEATURE_SIE)*/
#endif /*defined(MODEL_DEPENDENT)*/

    ARCH_DEP(program_interrupt)(regs, PGM_OPERATION_EXCEPTION);
}


DEF_INST(dummy_instruction)
{
    logmsg(_("Dummy instruction: ")); ARCH_DEP(display_inst) (regs, inst);

    if( !execflag )
    {
        regs->psw.ilc = (inst[0] < 0x40) ? 2 :
                        (inst[0] < 0xC0) ? 4 : 6;
        regs->psw.IA += regs->psw.ilc;
        regs->psw.IA &= ADDRESS_MAXWRAP(regs);
    }

}


#if !defined(_GEN_ARCH)

#define DISASM_ROUTE(_table,_route) \
void disasm_ ## _table (BYTE inst[], BYTE unused[]) \
{ \
func disasm_fn; \
BYTE* mnemonic; \
    UNREFERENCED(unused); \
    mnemonic = (void*)opcode_ ## _table [inst _route ][GEN_MAXARCH-1]; \
    disasm_fn = (void*)opcode_ ## _table [inst _route ][GEN_MAXARCH-2]; \
    disasm_fn(inst, mnemonic); \
}


DISASM_ROUTE(table,[0]);
DISASM_ROUTE(01xx,[1]);
DISASM_ROUTE(a5xx,[1] & 0x0F);
DISASM_ROUTE(a7xx,[1] & 0x0F);
DISASM_ROUTE(b2xx,[1]);
DISASM_ROUTE(b3xx,[1]);
DISASM_ROUTE(b9xx,[1]);
DISASM_ROUTE(c0xx,[1] & 0x0F);
DISASM_ROUTE(e3xx,[5]);
DISASM_ROUTE(e5xx,[1]);
DISASM_ROUTE(ebxx,[5]);
DISASM_ROUTE(ecxx,[5]);
DISASM_ROUTE(edxx,[5]);

#if defined(FEATURE_VECTOR_FACILITY)
 #define opcode_a4xx v_opcode_a4xx
 DISASM_ROUTE(a4xx,[1]);
 #undef opcode_a4xx
 #define opcode_a6xx v_opcode_a6xx
 DISASM_ROUTE(a6xx,[1]);
 #undef opcode_a6xx
 #define opcode_e4xx v_opcode_e4xx
 DISASM_ROUTE(e4xx,[1]);
 #undef opcode_e4xx
#else /*defined(FEATURE_VECTOR_FACILITY)*/
 #define disasm_a4xx disasm_none
 #define disasm_a6xx disasm_none
 #define disasm_e4xx disasm_none
#endif /*defined(FEATURE_VECTOR_FACILITY)*/


void disasm_none (BYTE inst[], BYTE mnemonic[])
{
    UNREFERENCED(inst);
    logmsg("%s\n",mnemonic);
}

void disasm_E (BYTE inst[], BYTE mnemonic[])
{
    UNREFERENCED(inst);
    logmsg("%s\n",mnemonic);
}

void disasm_RR (BYTE inst[], BYTE mnemonic[])
{
int r1, r2;
    r1 = inst[1] >> 4;
    r2 = inst[1] & 0x0F;
    logmsg("%-6.6s%d,%d\n",mnemonic,r1,r2);
}

void disasm_RR_SVC (BYTE inst[], BYTE mnemonic[])
{
    logmsg("%-6.6s%d\n",mnemonic,inst[1]);
}

void disasm_RRE (BYTE inst[], BYTE mnemonic[])
{
int r1, r2;
    r1 = inst[3] >> 4;
    r2 = inst[3] & 0x0F;
    logmsg("%-6.6s%d,%d\n",mnemonic,r1,r2);
}

void disasm_RRF_R (BYTE inst[], BYTE mnemonic[])
{
int r1,r3,r2;
    r1 = inst[2] >> 4;
    r3 = inst[3] >> 4;
    r2 = inst[3] & 0x0F;
    logmsg("%-6.6s%d,%d,%d\n",mnemonic,r1,r3,r2);
}

void disasm_RRF_M (BYTE inst[], BYTE mnemonic[])
{
int m3,r1,r2;
    m3 = inst[2] >> 4;
    r1 = inst[3] >> 4;
    r2 = inst[3] & 0x0F;
    logmsg("%-6.6s%d,%d,%d\n",mnemonic,m3,r1,r2);
}

void disasm_RRF_RM (BYTE inst[], BYTE mnemonic[])
{
int r3,m4,r1,r2;
    r3 = inst[2] >> 4;
    m4 = inst[2] & 0x0F;
    r1 = inst[3] >> 4;
    r2 = inst[3] & 0x0F;
    logmsg("%-6.6s%d,%d,%d,%d\n",mnemonic,r3,m4,r1,r2);
}

void disasm_RX (BYTE inst[], BYTE mnemonic[])
{
int r1,x2,b2,d2;
    r1 = inst[1] >> 4;
    x2 = inst[1] & 0x0F;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d,%d(%d,%d)\n",mnemonic,r1,d2,x2,b2);
}

void disasm_RXE (BYTE inst[], BYTE mnemonic[])
{
int r1,x2,b2,d2;
    r1 = inst[1] >> 4;
    x2 = inst[1] & 0x0F;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d,%d(%d,%d)\n",mnemonic,r1,d2,x2,b2);
}

void disasm_RXF (BYTE inst[], BYTE mnemonic[])
{
int r1,r3,x2,b2,d2;
    r1 = inst[4] >> 4;
    r3 = inst[1] >> 4;
    x2 = inst[1] & 0x0F;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d,%d,%d(%d,%d)\n",mnemonic,r1,r3,d2,x2,b2);
}

void disasm_RS (BYTE inst[], BYTE mnemonic[])
{
int r1,r3,b2,d2;
    r1 = inst[1] >> 4;
    r3 = inst[1] & 0x0F;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d,%d,%d(%d)\n",mnemonic,r1,r3,d2,b2);
}

void disasm_RSE (BYTE inst[], BYTE mnemonic[])
{
int r1,r3,b2,d2;
    r1 = inst[1] >> 4;
    r3 = inst[1] & 0x0F;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d,%d,%d(%d)\n",mnemonic,r1,r3,d2,b2);
}

void disasm_RSL (BYTE inst[], BYTE mnemonic[])
{
int l1,b1,d1;
    l1 = inst[1] >> 4;
    b1 = inst[2] >> 4;
    d1 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d(%d,%d)\n",mnemonic,d1,l1+1,b1);
}

void disasm_RSI (BYTE inst[], BYTE mnemonic[])
{
int r1,r3,i2;
    r1 = inst[1] >> 4;
    r3 = inst[1] & 0x0F;
    i2 = (S16)(((U16)inst[2] << 8) | inst[3]);
    logmsg("%-6.6s%d,%d,%d\n",mnemonic,r1,r3,i2*2);
}

void disasm_RI (BYTE inst[], BYTE mnemonic[])
{
int r1,i2;
    r1 = inst[1] >> 4;
    i2 = (S16)(((U16)inst[2] << 8) | inst[3]);
    logmsg("%-6.6s%d,%d\n",mnemonic,r1,i2);
}

void disasm_RI_B (BYTE inst[], BYTE mnemonic[])
{
int r1,i2;
    r1 = inst[1] >> 4;
    i2 = (S16)(((U16)inst[2] << 8) | inst[3]);
    logmsg("%-6.6s%d,%d\n",mnemonic,r1,i2*2);
}

void disasm_RIE (BYTE inst[], BYTE mnemonic[])
{
int r1,r3,i2;
    r1 = inst[1] >> 4;
    r3 = inst[1] & 0x0F;
    i2 = (S16)(((U16)inst[2] << 8) | inst[3]);
    logmsg("%-6.6s%d,%d,%d\n",mnemonic,r1,r3,i2*2);
}

void disasm_RIL (BYTE inst[], BYTE mnemonic[])
{
int r1,i2;
    r1 = inst[1] >> 4;
    i2 = (S32)((((U32)inst[2] << 24) | ((U32)inst[3] << 16)
       | ((U32)inst[4] << 8)) | inst[5]);
    logmsg("%-6.6s%d,%lld\n",mnemonic,r1,i2*2LL);
}

void disasm_SI (BYTE inst[], BYTE mnemonic[])
{
int i2,b1,d1;
    i2 = inst[1];
    b1 = inst[2] >> 4;
    d1 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d(%d),%d\n",mnemonic,d1,b1,i2);
}

void disasm_S (BYTE inst[], BYTE mnemonic[])
{
int d2,b2;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d(%d)\n",mnemonic,d2,b2);
}

void disasm_SS (BYTE inst[], BYTE mnemonic[])
{
int l1,l2,b1,d1,b2,d2;
    l1 = inst[1] >> 4;
    l2 = inst[1] & 0x0F;
    b1 = inst[2] >> 4;
    d1 = (inst[2] & 0x0F) << 8 | inst[3];
    b2 = inst[4] >> 4;
    d2 = (inst[4] & 0x0F) << 8 | inst[5];
    logmsg("%-6.6s%d(%d,%d),%d(%d,%d)\n",mnemonic,d1,l1+1,b1,d2,l2+1,b2);
}

void disasm_SS_L (BYTE inst[], BYTE mnemonic[])
{
int l1,b1,d1,b2,d2;
    l1 = inst[1];
    b1 = inst[2] >> 4;
    d1 = (inst[2] & 0x0F) << 8 | inst[3];
    b2 = inst[4] >> 4;
    d2 = (inst[4] & 0x0F) << 8 | inst[5];
    logmsg("%-6.6s%d(%d,%d),%d(%d)\n",mnemonic,d1,l1+1,b1,d2,b2);
}

void disasm_SS_R (BYTE inst[], BYTE mnemonic[])
{
int r1,r3,b2,d2,b4,d4;
    r1 = inst[1] >> 4;
    r3 = inst[1] & 0x0F;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    b4 = inst[4] >> 4;
    d4 = (inst[4] & 0x0F) << 8 | inst[5];
    logmsg("%-6.6s%d,%d,%d(%d),%d(%d)\n",mnemonic,r1,r3,d2,b2,d4,b4);
}

void disasm_SS_I (BYTE inst[], BYTE mnemonic[])
{
int r1,i3,b2,d2,b4,d4;
    r1 = inst[1] >> 4;
    i3 = inst[1] & 0x0F;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    b4 = inst[4] >> 4;
    d4 = (inst[4] & 0x0F) << 8 | inst[5];
    logmsg("%-6.6s%d,%d(%d),%d(%d),%d\n",mnemonic,r1,d2,b2,d4,b4,i3);
}

void disasm_SSE (BYTE inst[], BYTE mnemonic[])
{
int b1,d1,b2,d2;
    b1 = inst[2] >> 4;
    d1 = (inst[2] & 0x0F) << 8 | inst[3];
    b2 = inst[4] >> 4;
    d2 = (inst[4] & 0x0F) << 8 | inst[5];
    logmsg("%-6.6s%d(%d),%d(%d)\n",mnemonic,d1,b1,d2,b2);
}

void disasm_VST (BYTE inst[], BYTE mnemonic[])
{
int vr3,rt2,vr1,rs2;
    vr3 = inst[2] >> 4;
    rt2 = inst[2] & 0x0F;
    vr1 = inst[3] >> 4;
    rs2 = inst[3] & 0x0F;
    logmsg("%-6.6s%d,%d,%d(%d)\n",mnemonic,vr1,vr3,rs2,rt2);
}

void disasm_VR (BYTE inst[], BYTE mnemonic[])
{
int vr1,fr3,gr2;
    fr3 = inst[2] >> 4;
    vr1 = inst[3] >> 4;
    gr2 = inst[3] & 0x0F;
    logmsg("%-5.5s,%d,%d,%d\n",mnemonic,vr1,fr3,gr2);
}

void disasm_VS (BYTE inst[], BYTE mnemonic[])
{
int rs2;
    rs2 = inst[3] & 0x0F;
    logmsg("%-6.6s%d\n",mnemonic,rs2);
}

void disasm_VRSE (BYTE inst[], BYTE mnemonic[])
{
int vr1,vr3,d2,b2;
    vr3 = inst[2] >> 4;
    vr1 = inst[3] >> 4;
    b2 = inst[4] >> 4;
    d2 = (inst[4] & 0x0F) << 8 | inst[5];
    logmsg("%-6.6s%d,%d,%d(%d)\n",mnemonic,vr1,vr3,d2,b2);
}

void disasm_S_NW (BYTE inst[], BYTE mnemonic[])
{
int d2,b2;
    b2 = inst[2] >> 4;
    d2 = (inst[2] & 0x0F) << 8 | inst[3];
    logmsg("%-6.6s%d(%d)\n",mnemonic,d2,b2);
}

/* Gabor Hoffer (performance option) */
zz_func s370_opcode_table[256];
zz_func s390_opcode_table[256];
zz_func z900_opcode_table[256];

zz_func opcode_table[256][GEN_MAXARCH] = {
 /*00*/   GENx___x___x___ ,
 /*01*/   GENx___x390x900 (execute_01xx,01xx,""),
 /*02*/   GENx___x___x___ ,
 /*03*/   GENx___x___x___ ,
 /*04*/   GENx370x390x900 (set_program_mask,RR,"SPM"),
 /*05*/   GENx370x390x900 (branch_and_link_register,RR,"BALR"),
 /*06*/   GENx370x390x900 (branch_on_count_register,RR,"BCTR"),
 /*07*/   GENx370x390x900 (branch_on_condition_register,RR,"BCR"),
 /*08*/   GENx370x___x___ (set_storage_key,RR,"SSK"),
 /*09*/   GENx370x___x___ (insert_storage_key,RR,"ISK"),
 /*0A*/   GENx370x390x900 (supervisor_call,RR_SVC,"SVC"),
 /*0B*/   GENx___x390x900 (branch_and_set_mode,RR,"BSM"),
 /*0C*/   GENx___x390x900 (branch_and_save_and_set_mode,RR,"BASSM"),
 /*0D*/   GENx370x390x900 (branch_and_save_register,RR,"BASR"),
 /*0E*/   GENx370x390x900 (move_long,RR,"MVCL"),
 /*0F*/   GENx370x390x900 (compare_logical_character_long,RR,"CLCL"),
 /*10*/   GENx370x390x900 (load_positive_register,RR,"LPR"),
 /*11*/   GENx370x390x900 (load_negative_register,RR,"LNR"),
 /*12*/   GENx370x390x900 (load_and_test_register,RR,"LTR"),
 /*13*/   GENx370x390x900 (load_complement_register,RR,"LCR"),
 /*14*/   GENx370x390x900 (and_register,RR,"NR"),
 /*15*/   GENx370x390x900 (compare_logical_register,RR,"CLR"),
 /*16*/   GENx370x390x900 (or_register,RR,"OR"),
 /*17*/   GENx370x390x900 (exclusive_or_register,RR,"XR"),
 /*18*/   GENx370x390x900 (load_register,RR,"LR"),
 /*19*/   GENx370x390x900 (compare_register,RR,"CR"),
 /*1A*/   GENx370x390x900 (add_register,RR,"AR"),
 /*1B*/   GENx370x390x900 (subtract_register,RR,"SR"),
 /*1C*/   GENx370x390x900 (multiply_register,RR,"MR"),
 /*1D*/   GENx370x390x900 (divide_register,RR,"DR"),
 /*1E*/   GENx370x390x900 (add_logical_register,RR,"ALR"),
 /*1F*/   GENx370x390x900 (subtract_logical_register,RR,"SLR"),
 /*20*/   GENx370x390x900 (load_positive_float_long_reg,RR,"LPDR"),
 /*21*/   GENx370x390x900 (load_negative_float_long_reg,RR,"LNDR"),
 /*22*/   GENx370x390x900 (load_and_test_float_long_reg,RR,"LTDR"),
 /*23*/   GENx370x390x900 (load_complement_float_long_reg,RR,"LCDR"),
 /*24*/   GENx370x390x900 (halve_float_long_reg,RR,"HDR"),
 /*25*/   GENx370x390x900 (round_float_long_reg,RR,"LRDR"),
 /*26*/   GENx370x390x900 (multiply_float_ext_reg,RR,"MXR"),
 /*27*/   GENx370x390x900 (multiply_float_long_to_ext_reg,RR,"MXDR"),
 /*28*/   GENx370x390x900 (load_float_long_reg,RR,"LDR"),
 /*29*/   GENx370x390x900 (compare_float_long_reg,RR,"CDR"),
 /*2A*/   GENx370x390x900 (add_float_long_reg,RR,"ADR"),
 /*2B*/   GENx370x390x900 (subtract_float_long_reg,RR,"SDR"),
 /*2C*/   GENx370x390x900 (multiply_float_long_reg,RR,"MDR"),
 /*2D*/   GENx370x390x900 (divide_float_long_reg,RR,"DDR"),
 /*2E*/   GENx370x390x900 (add_unnormal_float_long_reg,RR,"AWR"),
 /*2F*/   GENx370x390x900 (subtract_unnormal_float_long_reg,RR,"SWR"),
 /*30*/   GENx370x390x900 (load_positive_float_short_reg,RR,"LPER"),
 /*31*/   GENx370x390x900 (load_negative_float_short_reg,RR,"LNER"),
 /*32*/   GENx370x390x900 (load_and_test_float_short_reg,RR,"LTER"),
 /*33*/   GENx370x390x900 (load_complement_float_short_reg,RR,"LCER"),
 /*34*/   GENx370x390x900 (halve_float_short_reg,RR,"HER"),
 /*35*/   GENx370x390x900 (round_float_short_reg,RR,"LRER"),
 /*36*/   GENx370x390x900 (add_float_ext_reg,RR,"AXR"),
 /*37*/   GENx370x390x900 (subtract_float_ext_reg,RR,"SXR"),
 /*38*/   GENx370x390x900 (load_float_short_reg,RR,"LER"),
 /*39*/   GENx370x390x900 (compare_float_short_reg,RR,"CER"),
 /*3A*/   GENx370x390x900 (add_float_short_reg,RR,"AER"),
 /*3B*/   GENx370x390x900 (subtract_float_short_reg,RR,"SER"),
 /*3C*/   GENx370x390x900 (multiply_float_short_to_long_reg,RR,"MER"),
 /*3D*/   GENx370x390x900 (divide_float_short_reg,RR,"DER"),
 /*3E*/   GENx370x390x900 (add_unnormal_float_short_reg,RR,"AUR"),
 /*3F*/   GENx370x390x900 (subtract_unnormal_float_short_reg,RR,"SUR"),
 /*40*/   GENx370x390x900 (store_halfword,RX,"STH"),
 /*41*/   GENx370x390x900 (load_address,RX,"LA"),
 /*42*/   GENx370x390x900 (store_character,RX,"STC"),
 /*43*/   GENx370x390x900 (insert_character,RX,"IC"),
 /*44*/   GENx370x390x900 (execute,RX,"EX"),
 /*45*/   GENx370x390x900 (branch_and_link,RX,"BAL"),
 /*46*/   GENx370x390x900 (branch_on_count,RX,"BCT"),
 /*47*/   GENx370x390x900 (branch_on_condition,RX,"BC"),
 /*48*/   GENx370x390x900 (load_halfword,RX,"LH"),
 /*49*/   GENx370x390x900 (compare_halfword,RX,"CH"),
 /*4A*/   GENx370x390x900 (add_halfword,RX,"AH"),
 /*4B*/   GENx370x390x900 (subtract_halfword,RX,"SH"),
 /*4C*/   GENx370x390x900 (multiply_halfword,RX,"MH"),
 /*4D*/   GENx370x390x900 (branch_and_save,RX,"BAS"),
 /*4E*/   GENx370x390x900 (convert_to_decimal,RX,"CVD"),
 /*4F*/   GENx370x390x900 (convert_to_binary,RX,"CVB"),
 /*50*/   GENx370x390x900 (store,RX,"ST"),
 /*51*/   GENx___x390x900 (load_address_extended,RX,"LAE"),
 /*52*/   GENx___x___x___ ,
 /*53*/   GENx___x___x___ ,
 /*54*/   GENx370x390x900 (and,RX,"N"),
 /*55*/   GENx370x390x900 (compare_logical,RX,"CL"),
 /*56*/   GENx370x390x900 (or,RX,"O"),
 /*57*/   GENx370x390x900 (exclusive_or,RX,"X"),
 /*58*/   GENx370x390x900 (load,RX,"L"),
 /*59*/   GENx370x390x900 (compare,RX,"C"),
 /*5A*/   GENx370x390x900 (add,RX,"A"),
 /*5B*/   GENx370x390x900 (subtract,RX,"S"),
 /*5C*/   GENx370x390x900 (multiply,RX,"M"),
 /*5D*/   GENx370x390x900 (divide,RX,"D"),
 /*5E*/   GENx370x390x900 (add_logical,RX,"AL"),
 /*5F*/   GENx370x390x900 (subtract_logical,RX,"SL"),
 /*60*/   GENx370x390x900 (store_float_long,RX,"STD"),
 /*61*/   GENx___x___x___ ,
 /*62*/   GENx___x___x___ ,
 /*63*/   GENx___x___x___ ,
 /*64*/   GENx___x___x___ ,
 /*65*/   GENx___x___x___ ,
 /*66*/   GENx___x___x___ ,
 /*67*/   GENx370x390x900 (multiply_float_long_to_ext,RX,"MXD"),
 /*68*/   GENx370x390x900 (load_float_long,RX,"LD"),
 /*69*/   GENx370x390x900 (compare_float_long,RX,"CD"),
 /*6A*/   GENx370x390x900 (add_float_long,RX,"AD"),
 /*6B*/   GENx370x390x900 (subtract_float_long,RX,"SD"),
 /*6C*/   GENx370x390x900 (multiply_float_long,RX,"MD"),
 /*6D*/   GENx370x390x900 (divide_float_long,RX,"DD"),
 /*6E*/   GENx370x390x900 (add_unnormal_float_long,RX,"AW"),
 /*6F*/   GENx370x390x900 (subtract_unnormal_float_long,RX,"SW"),
 /*70*/   GENx370x390x900 (store_float_short,RX,"STE"),
 /*71*/   GENx___x390x900 (multiply_single,RX,"MS"),
 /*72*/   GENx___x___x___ ,
 /*73*/   GENx___x___x___ ,
 /*74*/   GENx___x___x___ ,
 /*75*/   GENx___x___x___ ,
 /*76*/   GENx___x___x___ ,
 /*77*/   GENx___x___x___ ,
 /*78*/   GENx370x390x900 (load_float_short,RX,"LE"),
 /*79*/   GENx370x390x900 (compare_float_short,RX,"CE"),
 /*7A*/   GENx370x390x900 (add_float_short,RX,"AE"),
 /*7B*/   GENx370x390x900 (subtract_float_short,RX,"SE"),
 /*7C*/   GENx370x390x900 (multiply_float_short_to_long,RX,"ME"),
 /*7D*/   GENx370x390x900 (divide_float_short,RX,"DE"),
 /*7E*/   GENx370x390x900 (add_unnormal_float_short,RX,"AU"),
 /*7F*/   GENx370x390x900 (subtract_unnormal_float_short,RX,"SU"),
 /*80*/   GENx370x390x900 (set_system_mask,S,"SSM"),
 /*81*/   GENx___x___x___ ,
 /*82*/   GENx370x390x900 (load_program_status_word,S,"LPSW"),
 /*83*/   GENx370x390x900 (diagnose,RS,"DIAG"),
 /*84*/   GENx___x390x900 (branch_relative_on_index_high,RSI,"BRXH"),
 /*85*/   GENx___x390x900 (branch_relative_on_index_low_or_equal,RSI,"BRXLE"),
 /*86*/   GENx370x390x900 (branch_on_index_high,RS,"BXH"),
 /*87*/   GENx370x390x900 (branch_on_index_low_or_equal,RS,"BXLE"),
 /*88*/   GENx370x390x900 (shift_right_single_logical,RS,"SRL"),
 /*89*/   GENx370x390x900 (shift_left_single_logical,RS,"SLL"),
 /*8A*/   GENx370x390x900 (shift_right_single,RS,"SRA"),
 /*8B*/   GENx370x390x900 (shift_left_single,RS,"SLA"),
 /*8C*/   GENx370x390x900 (shift_right_double_logical,RS,"SRDL"),
 /*8D*/   GENx370x390x900 (shift_left_double_logical,RS,"SLDL"),
 /*8E*/   GENx370x390x900 (shift_right_double,RS,"SRDA"),
 /*8F*/   GENx370x390x900 (shift_left_double,RS,"SLDA"),
 /*90*/   GENx370x390x900 (store_multiple,RS,"STM"),
 /*91*/   GENx370x390x900 (test_under_mask,SI,"TM"),
 /*92*/   GENx370x390x900 (move_immediate,SI,"MVI"),
 /*93*/   GENx370x390x900 (test_and_set,SI,"TS"),
 /*94*/   GENx370x390x900 (and_immediate,SI,"NI"),
 /*95*/   GENx370x390x900 (compare_logical_immediate,SI,"CLI"),
 /*96*/   GENx370x390x900 (or_immediate,SI,"OI"),
 /*97*/   GENx370x390x900 (exclusive_or_immediate,SI,"XI"),
 /*98*/   GENx370x390x900 (load_multiple,RS,"LM"),
 /*99*/   GENx___x390x900 (trace,RS,"TRACE"),
 /*9A*/   GENx___x390x900 (load_access_multiple,RS,"LAM"),
 /*9B*/   GENx___x390x900 (store_access_multiple,RS,"STAM"),
 /*9C*/   GENx370x___x___ (start_io,S,"SIO"),
 /*9D*/   GENx370x___x___ (test_io,S,"TIO"),
 /*9E*/   GENx370x___x___ (halt_io,S,"HIO"),
 /*9F*/   GENx370x___x___ (test_channel,S,"TCH"),
 /*A0*/   GENx___x___x___ ,
 /*A1*/   GENx___x___x___ ,
 /*A2*/   GENx___x___x___ ,
 /*A3*/   GENx___x___x___ ,
 /*A4*/   GENx370x390x___ (execute_a4xx,a4xx,""),
 /*A5*/   GENx370x390x900 (execute_a5xx,a5xx,""),
 /*A6*/   GENx370x390x___ (execute_a6xx,a6xx,""),
 /*A7*/   GENx___x390x900 (execute_a7xx,a7xx,""),
 /*A8*/   GENx___x390x900 (move_long_extended,RS,"MVCLE"),
 /*A9*/   GENx___x390x900 (compare_logical_long_extended,RS,"CLCLE"),
 /*AA*/   GENx___x___x___ ,
 /*AB*/   GENx___x___x___ ,
 /*AC*/   GENx370x390x900 (store_then_and_system_mask,SI,"STNSM"),
 /*AD*/   GENx370x390x900 (store_then_or_system_mask,SI,"STOSM"),
 /*AE*/   GENx370x390x900 (signal_procesor,RS,"SIGP"),
 /*AF*/   GENx370x390x900 (monitor_call,SI,"MC"),
 /*B0*/   GENx___x___x___ ,
 /*B1*/   GENx370x390x900 (load_real_address,RX,"LRA"),
 /*B2*/   GENx370x390x900 (execute_b2xx,b2xx,""),
 /*B3*/   GENx___x390x900 (execute_b3xx,b3xx,""),
 /*B4*/   GENx___x___x___ ,
 /*B5*/   GENx___x___x___ ,
 /*B6*/   GENx370x390x900 (store_control,RS,"STCTL"),
 /*B7*/   GENx370x390x900 (load_control,RS,"LCTL"),
 /*B8*/   GENx___x___x___ ,
 /*B9*/   GENx___x390x900 (execute_b9xx,b9xx,""),
 /*BA*/   GENx370x390x900 (compare_and_swap,RS,"CS"),
 /*BB*/   GENx370x390x900 (compare_double_and_swap,RS,"CDS"),
 /*BC*/   GENx___x___x___ ,
 /*BD*/   GENx370x390x900 (compare_logical_characters_under_mask,RS,"CLM"),
 /*BE*/   GENx370x390x900 (store_characters_under_mask,RS,"STCM"),
 /*BF*/   GENx370x390x900 (insert_characters_under_mask,RS,"ICM"),
 /*C0*/   GENx___x390x900 (execute_c0xx,c0xx,""),
 /*C1*/   GENx___x___x___ ,
 /*C2*/   GENx___x___x___ ,
 /*C3*/   GENx___x___x___ ,
 /*C4*/   GENx___x___x___ ,
 /*C5*/   GENx___x___x___ ,
 /*C6*/   GENx___x___x___ ,
 /*C7*/   GENx___x___x___ ,
 /*C8*/   GENx___x___x___ ,
 /*C9*/   GENx___x___x___ ,
 /*CA*/   GENx___x___x___ ,
 /*CB*/   GENx___x___x___ ,
 /*CC*/   GENx___x___x___ ,
 /*CD*/   GENx___x___x___ ,
 /*CE*/   GENx___x___x___ ,
 /*CF*/   GENx___x___x___ ,
 /*D0*/   GENx___x___x___ ,
 /*D1*/   GENx370x390x900 (move_numerics,SS_L,"MVN"),
 /*D2*/   GENx370x390x900 (move_character,SS_L,"MVC"),
 /*D3*/   GENx370x390x900 (move_zones,SS_L,"MVZ"),
 /*D4*/   GENx370x390x900 (and_character,SS_L,"NC"),
 /*D5*/   GENx370x390x900 (compare_logical_character,SS_L,"CLC"),
 /*D6*/   GENx370x390x900 (or_character,SS_L,"OC"),
 /*D7*/   GENx370x390x900 (exclusive_or_character,SS_L,"XC"),
 /*D8*/   GENx___x___x___ ,
 /*D9*/   GENx370x390x900 (move_with_key,SS,"MVCK"),
 /*DA*/   GENx370x390x900 (move_to_primary,SS,"MVCP"),
 /*DB*/   GENx370x390x900 (move_to_secondary,SS,"MVCS"),
 /*DC*/   GENx370x390x900 (translate,SS_L,"TR"),
 /*DD*/   GENx370x390x900 (translate_and_test,SS_L,"TRT"),
 /*DE*/   GENx370x390x900 (edit_x_edit_and_mark,SS_L,"ED"),
 /*DF*/   GENx370x390x900 (edit_x_edit_and_mark,SS_L,"EDMK"),
 /*E0*/   GENx___x___x___ ,
 /*E1*/   GENx___x___x900 (pack_unicode,SS_L,"PKU"),
 /*E2*/   GENx___x___x900 (unpack_unicode,SS_L,"UNPKU"),
 /*E3*/   GENx___x390x900 (execute_e3xx,e3xx,""),
 /*E4*/   GENx370x390x___ (execute_e4xx,e4xx,""),
 /*E5*/   GENx370x390x900 (execute_e5xx,e5xx,""),
 /*E6*/   GENx___x___x___ ,
 /*E7*/   GENx___x___x___ ,
 /*E8*/   GENx370x390x900 (move_inverse,SS_L,"MVCIN"),
 /*E9*/   GENx___x___x900 (pack_ascii,SS_L,"PKA"),
 /*EA*/   GENx___x___x900 (unpack_ascii,SS_L,"UNPKA"),
 /*EB*/   GENx___x390x900 (execute_ebxx,ebxx,""),
 /*EC*/   GENx___x390x900 (execute_ecxx,ecxx,""),
 /*ED*/   GENx___x390x900 (execute_edxx,edxx,""),
 /*EE*/   GENx___x390x900 (perform_locked_operation,SS_R,"PLO"),
 /*EF*/   GENx___x___x900 (load_multiple_disjoint,SS_R,"LMD"),
 /*F0*/   GENx370x390x900 (shift_and_round_decimal,SS_I,"SRP"),
 /*F1*/   GENx370x390x900 (move_with_offset,SS,"MVO"),
 /*F2*/   GENx370x390x900 (pack,SS,"PACK"),
 /*F3*/   GENx370x390x900 (unpack,SS,"UNPK"),
 /*F4*/   GENx___x___x___ ,
 /*F5*/   GENx___x___x___ ,
 /*F6*/   GENx___x___x___ ,
 /*F7*/   GENx___x___x___ ,
 /*F8*/   GENx370x390x900 (zero_and_add,SS,"ZAP"),
 /*F9*/   GENx370x390x900 (compare_decimal,SS,"CP"),
 /*FA*/   GENx370x390x900 (add_decimal,SS,"AP"),
 /*FB*/   GENx370x390x900 (subtract_decimal,SS,"SP"),
 /*FC*/   GENx370x390x900 (multiply_decimal,SS,"MP"),
 /*FD*/   GENx370x390x900 (divide_decimal,SS,"DP"),
 /*FE*/   GENx___x___x___ ,
 /*FF*/   GENx___x___x___  };


zz_func opcode_01xx[256][GEN_MAXARCH] = {
 /*0100*/ GENx___x___x___ ,
 /*0101*/ GENx___x390x900 (program_return,E,"PR"),
 /*0102*/ GENx___x390x900 (update_tree,E,"UPT"),
 /*0103*/ GENx___x___x___ ,
 /*0104*/ GENx___x___x___ ,
 /*0105*/ GENx___x___x___ ,                                     /* CMSG      */
 /*0106*/ GENx___x___x___ ,                                     /* TMSG      */
 /*0107*/ GENx___x390x900 (set_clock_programmable_field,E,"SCKPF"),
 /*0108*/ GENx___x___x___ ,                                     /* TMPS      */
 /*0109*/ GENx___x___x___ ,                                     /* CMPS      */
 /*010A*/ GENx___x___x___ ,
 /*010B*/ GENx___x390x900 (test_addressing_mode,E,"TAM"),
 /*010C*/ GENx___x390x900 (set_addressing_mode_24,E,"SAM24"),
 /*010D*/ GENx___x390x900 (set_addressing_mode_31,E,"SAM31"),
 /*010E*/ GENx___x___x900 (set_addressing_mode_64,E,"SAM64"),
 /*010F*/ GENx___x___x___ ,
 /*0110*/ GENx___x___x___ ,
 /*0111*/ GENx___x___x___ ,
 /*0112*/ GENx___x___x___ ,
 /*0113*/ GENx___x___x___ ,
 /*0114*/ GENx___x___x___ ,
 /*0115*/ GENx___x___x___ ,
 /*0116*/ GENx___x___x___ ,
 /*0117*/ GENx___x___x___ ,
 /*0118*/ GENx___x___x___ ,
 /*0119*/ GENx___x___x___ ,
 /*011A*/ GENx___x___x___ ,
 /*011B*/ GENx___x___x___ ,
 /*011C*/ GENx___x___x___ ,
 /*011D*/ GENx___x___x___ ,
 /*011E*/ GENx___x___x___ ,
 /*011F*/ GENx___x___x___ ,
 /*0120*/ GENx___x___x___ ,
 /*0121*/ GENx___x___x___ ,
 /*0122*/ GENx___x___x___ ,
 /*0123*/ GENx___x___x___ ,
 /*0124*/ GENx___x___x___ ,
 /*0125*/ GENx___x___x___ ,
 /*0126*/ GENx___x___x___ ,
 /*0127*/ GENx___x___x___ ,
 /*0128*/ GENx___x___x___ ,
 /*0129*/ GENx___x___x___ ,
 /*012A*/ GENx___x___x___ ,
 /*012B*/ GENx___x___x___ ,
 /*012C*/ GENx___x___x___ ,
 /*012D*/ GENx___x___x___ ,
 /*012E*/ GENx___x___x___ ,
 /*012F*/ GENx___x___x___ ,
 /*0130*/ GENx___x___x___ ,
 /*0131*/ GENx___x___x___ ,
 /*0132*/ GENx___x___x___ ,
 /*0133*/ GENx___x___x___ ,
 /*0134*/ GENx___x___x___ ,
 /*0135*/ GENx___x___x___ ,
 /*0136*/ GENx___x___x___ ,
 /*0137*/ GENx___x___x___ ,
 /*0138*/ GENx___x___x___ ,
 /*0139*/ GENx___x___x___ ,
 /*013A*/ GENx___x___x___ ,
 /*013B*/ GENx___x___x___ ,
 /*013C*/ GENx___x___x___ ,
 /*013D*/ GENx___x___x___ ,
 /*013E*/ GENx___x___x___ ,
 /*013F*/ GENx___x___x___ ,
 /*0140*/ GENx___x___x___ ,
 /*0141*/ GENx___x___x___ ,
 /*0142*/ GENx___x___x___ ,
 /*0143*/ GENx___x___x___ ,
 /*0144*/ GENx___x___x___ ,
 /*0145*/ GENx___x___x___ ,
 /*0146*/ GENx___x___x___ ,
 /*0147*/ GENx___x___x___ ,
 /*0148*/ GENx___x___x___ ,
 /*0149*/ GENx___x___x___ ,
 /*014A*/ GENx___x___x___ ,
 /*014B*/ GENx___x___x___ ,
 /*014C*/ GENx___x___x___ ,
 /*014D*/ GENx___x___x___ ,
 /*014E*/ GENx___x___x___ ,
 /*014F*/ GENx___x___x___ ,
 /*0150*/ GENx___x___x___ ,
 /*0151*/ GENx___x___x___ ,
 /*0152*/ GENx___x___x___ ,
 /*0153*/ GENx___x___x___ ,
 /*0154*/ GENx___x___x___ ,
 /*0155*/ GENx___x___x___ ,
 /*0156*/ GENx___x___x___ ,
 /*0157*/ GENx___x___x___ ,
 /*0158*/ GENx___x___x___ ,
 /*0159*/ GENx___x___x___ ,
 /*015A*/ GENx___x___x___ ,
 /*015B*/ GENx___x___x___ ,
 /*015C*/ GENx___x___x___ ,
 /*015D*/ GENx___x___x___ ,
 /*015E*/ GENx___x___x___ ,
 /*015F*/ GENx___x___x___ ,
 /*0160*/ GENx___x___x___ ,
 /*0161*/ GENx___x___x___ ,
 /*0162*/ GENx___x___x___ ,
 /*0163*/ GENx___x___x___ ,
 /*0164*/ GENx___x___x___ ,
 /*0165*/ GENx___x___x___ ,
 /*0166*/ GENx___x___x___ ,
 /*0167*/ GENx___x___x___ ,
 /*0168*/ GENx___x___x___ ,
 /*0169*/ GENx___x___x___ ,
 /*016A*/ GENx___x___x___ ,
 /*016B*/ GENx___x___x___ ,
 /*016C*/ GENx___x___x___ ,
 /*016D*/ GENx___x___x___ ,
 /*016E*/ GENx___x___x___ ,
 /*016F*/ GENx___x___x___ ,
 /*0170*/ GENx___x___x___ ,
 /*0171*/ GENx___x___x___ ,
 /*0172*/ GENx___x___x___ ,
 /*0173*/ GENx___x___x___ ,
 /*0174*/ GENx___x___x___ ,
 /*0175*/ GENx___x___x___ ,
 /*0176*/ GENx___x___x___ ,
 /*0177*/ GENx___x___x___ ,
 /*0178*/ GENx___x___x___ ,
 /*0179*/ GENx___x___x___ ,
 /*017A*/ GENx___x___x___ ,
 /*017B*/ GENx___x___x___ ,
 /*017C*/ GENx___x___x___ ,
 /*017D*/ GENx___x___x___ ,
 /*017E*/ GENx___x___x___ ,
 /*017F*/ GENx___x___x___ ,
 /*0180*/ GENx___x___x___ ,
 /*0181*/ GENx___x___x___ ,
 /*0182*/ GENx___x___x___ ,
 /*0183*/ GENx___x___x___ ,
 /*0184*/ GENx___x___x___ ,
 /*0185*/ GENx___x___x___ ,
 /*0186*/ GENx___x___x___ ,
 /*0187*/ GENx___x___x___ ,
 /*0188*/ GENx___x___x___ ,
 /*0189*/ GENx___x___x___ ,
 /*018A*/ GENx___x___x___ ,
 /*018B*/ GENx___x___x___ ,
 /*018C*/ GENx___x___x___ ,
 /*018D*/ GENx___x___x___ ,
 /*018E*/ GENx___x___x___ ,
 /*018F*/ GENx___x___x___ ,
 /*0190*/ GENx___x___x___ ,
 /*0191*/ GENx___x___x___ ,
 /*0192*/ GENx___x___x___ ,
 /*0193*/ GENx___x___x___ ,
 /*0194*/ GENx___x___x___ ,
 /*0195*/ GENx___x___x___ ,
 /*0196*/ GENx___x___x___ ,
 /*0197*/ GENx___x___x___ ,
 /*0198*/ GENx___x___x___ ,
 /*0199*/ GENx___x___x___ ,
 /*019A*/ GENx___x___x___ ,
 /*019B*/ GENx___x___x___ ,
 /*019C*/ GENx___x___x___ ,
 /*019D*/ GENx___x___x___ ,
 /*019E*/ GENx___x___x___ ,
 /*019F*/ GENx___x___x___ ,
 /*01A0*/ GENx___x___x___ ,
 /*01A1*/ GENx___x___x___ ,
 /*01A2*/ GENx___x___x___ ,
 /*01A3*/ GENx___x___x___ ,
 /*01A4*/ GENx___x___x___ ,
 /*01A5*/ GENx___x___x___ ,
 /*01A6*/ GENx___x___x___ ,
 /*01A7*/ GENx___x___x___ ,
 /*01A8*/ GENx___x___x___ ,
 /*01A9*/ GENx___x___x___ ,
 /*01AA*/ GENx___x___x___ ,
 /*01AB*/ GENx___x___x___ ,
 /*01AC*/ GENx___x___x___ ,
 /*01AD*/ GENx___x___x___ ,
 /*01AE*/ GENx___x___x___ ,
 /*01AF*/ GENx___x___x___ ,
 /*01B0*/ GENx___x___x___ ,
 /*01B1*/ GENx___x___x___ ,
 /*01B2*/ GENx___x___x___ ,
 /*01B3*/ GENx___x___x___ ,
 /*01B4*/ GENx___x___x___ ,
 /*01B5*/ GENx___x___x___ ,
 /*01B6*/ GENx___x___x___ ,
 /*01B7*/ GENx___x___x___ ,
 /*01B8*/ GENx___x___x___ ,
 /*01B9*/ GENx___x___x___ ,
 /*01BA*/ GENx___x___x___ ,
 /*01BB*/ GENx___x___x___ ,
 /*01BC*/ GENx___x___x___ ,
 /*01BD*/ GENx___x___x___ ,
 /*01BE*/ GENx___x___x___ ,
 /*01BF*/ GENx___x___x___ ,
 /*01C0*/ GENx___x___x___ ,
 /*01C1*/ GENx___x___x___ ,
 /*01C2*/ GENx___x___x___ ,
 /*01C3*/ GENx___x___x___ ,
 /*01C4*/ GENx___x___x___ ,
 /*01C5*/ GENx___x___x___ ,
 /*01C6*/ GENx___x___x___ ,
 /*01C7*/ GENx___x___x___ ,
 /*01C8*/ GENx___x___x___ ,
 /*01C9*/ GENx___x___x___ ,
 /*01CA*/ GENx___x___x___ ,
 /*01CB*/ GENx___x___x___ ,
 /*01CC*/ GENx___x___x___ ,
 /*01CD*/ GENx___x___x___ ,
 /*01CE*/ GENx___x___x___ ,
 /*01CF*/ GENx___x___x___ ,
 /*01D0*/ GENx___x___x___ ,
 /*01D1*/ GENx___x___x___ ,
 /*01D2*/ GENx___x___x___ ,
 /*01D3*/ GENx___x___x___ ,
 /*01D4*/ GENx___x___x___ ,
 /*01D5*/ GENx___x___x___ ,
 /*01D6*/ GENx___x___x___ ,
 /*01D7*/ GENx___x___x___ ,
 /*01D8*/ GENx___x___x___ ,
 /*01D9*/ GENx___x___x___ ,
 /*01DA*/ GENx___x___x___ ,
 /*01DB*/ GENx___x___x___ ,
 /*01DC*/ GENx___x___x___ ,
 /*01DD*/ GENx___x___x___ ,
 /*01DE*/ GENx___x___x___ ,
 /*01DF*/ GENx___x___x___ ,
 /*01E0*/ GENx___x___x___ ,
 /*01E1*/ GENx___x___x___ ,
 /*01E2*/ GENx___x___x___ ,
 /*01E3*/ GENx___x___x___ ,
 /*01E4*/ GENx___x___x___ ,
 /*01E5*/ GENx___x___x___ ,
 /*01E6*/ GENx___x___x___ ,
 /*01E7*/ GENx___x___x___ ,
 /*01E8*/ GENx___x___x___ ,
 /*01E9*/ GENx___x___x___ ,
 /*01EA*/ GENx___x___x___ ,
 /*01EB*/ GENx___x___x___ ,
 /*01EC*/ GENx___x___x___ ,
 /*01ED*/ GENx___x___x___ ,
 /*01EE*/ GENx___x___x___ ,
 /*01EF*/ GENx___x___x___ ,
 /*01F0*/ GENx___x___x___ ,
 /*01F1*/ GENx___x___x___ ,
 /*01F2*/ GENx___x___x___ ,
 /*01F3*/ GENx___x___x___ ,
 /*01F4*/ GENx___x___x___ ,
 /*01F5*/ GENx___x___x___ ,
 /*01F6*/ GENx___x___x___ ,
 /*01F7*/ GENx___x___x___ ,
 /*01F8*/ GENx___x___x___ ,
 /*01F9*/ GENx___x___x___ ,
 /*01FA*/ GENx___x___x___ ,
 /*01FB*/ GENx___x___x___ ,
 /*01FC*/ GENx___x___x___ ,
 /*01FD*/ GENx___x___x___ ,
 /*01FE*/ GENx___x___x___ ,
 /*01FF*/ GENx___x390x900 (trap2,E,"TRAP") };


// #if defined(FEATURE_ESAME)

zz_func opcode_a4xx[256][GEN_MAXARCH] = {
 /*A400*/ GENx___x___x___ ,
 /*A401*/ GENx___x___x___ ,
 /*A402*/ GENx___x___x___ ,
 /*A403*/ GENx___x___x___ ,
 /*A404*/ GENx___x___x___ ,
 /*A405*/ GENx___x___x___ ,
 /*A406*/ GENx___x___x___ ,
 /*A407*/ GENx___x___x___ ,
 /*A408*/ GENx___x___x___ ,
 /*A409*/ GENx___x___x___ ,
 /*A40A*/ GENx___x___x___ ,
 /*A40B*/ GENx___x___x___ ,
 /*A40C*/ GENx___x___x___ ,
 /*A40D*/ GENx___x___x___ ,
 /*A40E*/ GENx___x___x___ ,
 /*A40F*/ GENx___x___x___ ,
 /*A410*/ GENx___x___x___ ,
 /*A411*/ GENx___x___x___ ,
 /*A412*/ GENx___x___x___ ,
 /*A413*/ GENx___x___x___ ,
 /*A414*/ GENx___x___x___ ,
 /*A415*/ GENx___x___x___ ,
 /*A416*/ GENx___x___x___ ,
 /*A417*/ GENx___x___x___ ,
 /*A418*/ GENx___x___x___ ,
 /*A419*/ GENx___x___x___ ,
 /*A41A*/ GENx___x___x___ ,
 /*A41B*/ GENx___x___x___ ,
 /*A41C*/ GENx___x___x___ ,
 /*A41D*/ GENx___x___x___ ,
 /*A41E*/ GENx___x___x___ ,
 /*A41F*/ GENx___x___x___ ,
 /*A420*/ GENx___x___x___ ,
 /*A421*/ GENx___x___x___ ,
 /*A422*/ GENx___x___x___ ,
 /*A423*/ GENx___x___x___ ,
 /*A424*/ GENx___x___x___ ,
 /*A425*/ GENx___x___x___ ,
 /*A426*/ GENx___x___x___ ,
 /*A427*/ GENx___x___x___ ,
 /*A428*/ GENx___x___x___ ,
 /*A429*/ GENx___x___x___ ,
 /*A42A*/ GENx___x___x___ ,
 /*A42B*/ GENx___x___x___ ,
 /*A42C*/ GENx___x___x___ ,
 /*A42D*/ GENx___x___x___ ,
 /*A42E*/ GENx___x___x___ ,
 /*A42F*/ GENx___x___x___ ,
 /*A430*/ GENx___x___x___ ,
 /*A431*/ GENx___x___x___ ,
 /*A432*/ GENx___x___x___ ,
 /*A433*/ GENx___x___x___ ,
 /*A434*/ GENx___x___x___ ,
 /*A435*/ GENx___x___x___ ,
 /*A436*/ GENx___x___x___ ,
 /*A437*/ GENx___x___x___ ,
 /*A438*/ GENx___x___x___ ,
 /*A439*/ GENx___x___x___ ,
 /*A43A*/ GENx___x___x___ ,
 /*A43B*/ GENx___x___x___ ,
 /*A43C*/ GENx___x___x___ ,
 /*A43D*/ GENx___x___x___ ,
 /*A43E*/ GENx___x___x___ ,
 /*A43F*/ GENx___x___x___ ,
 /*A440*/ GENx___x___x___ ,
 /*A441*/ GENx___x___x___ ,
 /*A442*/ GENx___x___x___ ,
 /*A443*/ GENx___x___x___ ,
 /*A444*/ GENx___x___x___ ,
 /*A445*/ GENx___x___x___ ,
 /*A446*/ GENx___x___x___ ,
 /*A447*/ GENx___x___x___ ,
 /*A448*/ GENx___x___x___ ,
 /*A449*/ GENx___x___x___ ,
 /*A44A*/ GENx___x___x___ ,
 /*A44B*/ GENx___x___x___ ,
 /*A44C*/ GENx___x___x___ ,
 /*A44D*/ GENx___x___x___ ,
 /*A44E*/ GENx___x___x___ ,
 /*A44F*/ GENx___x___x___ ,
 /*A450*/ GENx___x___x___ ,
 /*A451*/ GENx___x___x___ ,
 /*A452*/ GENx___x___x___ ,
 /*A453*/ GENx___x___x___ ,
 /*A454*/ GENx___x___x___ ,
 /*A455*/ GENx___x___x___ ,
 /*A456*/ GENx___x___x___ ,
 /*A457*/ GENx___x___x___ ,
 /*A458*/ GENx___x___x___ ,
 /*A459*/ GENx___x___x___ ,
 /*A45A*/ GENx___x___x___ ,
 /*A45B*/ GENx___x___x___ ,
 /*A45C*/ GENx___x___x___ ,
 /*A45D*/ GENx___x___x___ ,
 /*A45E*/ GENx___x___x___ ,
 /*A45F*/ GENx___x___x___ ,
 /*A460*/ GENx___x___x___ ,
 /*A461*/ GENx___x___x___ ,
 /*A462*/ GENx___x___x___ ,
 /*A463*/ GENx___x___x___ ,
 /*A464*/ GENx___x___x___ ,
 /*A465*/ GENx___x___x___ ,
 /*A466*/ GENx___x___x___ ,
 /*A467*/ GENx___x___x___ ,
 /*A468*/ GENx___x___x___ ,
 /*A469*/ GENx___x___x___ ,
 /*A46A*/ GENx___x___x___ ,
 /*A46B*/ GENx___x___x___ ,
 /*A46C*/ GENx___x___x___ ,
 /*A46D*/ GENx___x___x___ ,
 /*A46E*/ GENx___x___x___ ,
 /*A46F*/ GENx___x___x___ ,
 /*A470*/ GENx___x___x___ ,
 /*A471*/ GENx___x___x___ ,
 /*A472*/ GENx___x___x___ ,
 /*A473*/ GENx___x___x___ ,
 /*A474*/ GENx___x___x___ ,
 /*A475*/ GENx___x___x___ ,
 /*A476*/ GENx___x___x___ ,
 /*A477*/ GENx___x___x___ ,
 /*A478*/ GENx___x___x___ ,
 /*A479*/ GENx___x___x___ ,
 /*A47A*/ GENx___x___x___ ,
 /*A47B*/ GENx___x___x___ ,
 /*A47C*/ GENx___x___x___ ,
 /*A47D*/ GENx___x___x___ ,
 /*A47E*/ GENx___x___x___ ,
 /*A47F*/ GENx___x___x___ ,
 /*A480*/ GENx___x___x___ ,
 /*A481*/ GENx___x___x___ ,
 /*A482*/ GENx___x___x___ ,
 /*A483*/ GENx___x___x___ ,
 /*A484*/ GENx___x___x___ ,
 /*A485*/ GENx___x___x___ ,
 /*A486*/ GENx___x___x___ ,
 /*A487*/ GENx___x___x___ ,
 /*A488*/ GENx___x___x___ ,
 /*A489*/ GENx___x___x___ ,
 /*A48A*/ GENx___x___x___ ,
 /*A48B*/ GENx___x___x___ ,
 /*A48C*/ GENx___x___x___ ,
 /*A48D*/ GENx___x___x___ ,
 /*A48E*/ GENx___x___x___ ,
 /*A48F*/ GENx___x___x___ ,
 /*A490*/ GENx___x___x___ ,
 /*A491*/ GENx___x___x___ ,
 /*A492*/ GENx___x___x___ ,
 /*A493*/ GENx___x___x___ ,
 /*A494*/ GENx___x___x___ ,
 /*A495*/ GENx___x___x___ ,
 /*A496*/ GENx___x___x___ ,
 /*A497*/ GENx___x___x___ ,
 /*A498*/ GENx___x___x___ ,
 /*A499*/ GENx___x___x___ ,
 /*A49A*/ GENx___x___x___ ,
 /*A49B*/ GENx___x___x___ ,
 /*A49C*/ GENx___x___x___ ,
 /*A49D*/ GENx___x___x___ ,
 /*A49E*/ GENx___x___x___ ,
 /*A49F*/ GENx___x___x___ ,
 /*A4A0*/ GENx___x___x___ ,
 /*A4A1*/ GENx___x___x___ ,
 /*A4A2*/ GENx___x___x___ ,
 /*A4A3*/ GENx___x___x___ ,
 /*A4A4*/ GENx___x___x___ ,
 /*A4A5*/ GENx___x___x___ ,
 /*A4A6*/ GENx___x___x___ ,
 /*A4A7*/ GENx___x___x___ ,
 /*A4A8*/ GENx___x___x___ ,
 /*A4A9*/ GENx___x___x___ ,
 /*A4AA*/ GENx___x___x___ ,
 /*A4AB*/ GENx___x___x___ ,
 /*A4AC*/ GENx___x___x___ ,
 /*A4AD*/ GENx___x___x___ ,
 /*A4AE*/ GENx___x___x___ ,
 /*A4AF*/ GENx___x___x___ ,
 /*A4B0*/ GENx___x___x___ ,
 /*A4B1*/ GENx___x___x___ ,
 /*A4B2*/ GENx___x___x___ ,
 /*A4B3*/ GENx___x___x___ ,
 /*A4B4*/ GENx___x___x___ ,
 /*A4B5*/ GENx___x___x___ ,
 /*A4B6*/ GENx___x___x___ ,
 /*A4B7*/ GENx___x___x___ ,
 /*A4B8*/ GENx___x___x___ ,
 /*A4B9*/ GENx___x___x___ ,
 /*A4BA*/ GENx___x___x___ ,
 /*A4BB*/ GENx___x___x___ ,
 /*A4BC*/ GENx___x___x___ ,
 /*A4BD*/ GENx___x___x___ ,
 /*A4BE*/ GENx___x___x___ ,
 /*A4BF*/ GENx___x___x___ ,
 /*A4C0*/ GENx___x___x___ ,
 /*A4C1*/ GENx___x___x___ ,
 /*A4C2*/ GENx___x___x___ ,
 /*A4C3*/ GENx___x___x___ ,
 /*A4C4*/ GENx___x___x___ ,
 /*A4C5*/ GENx___x___x___ ,
 /*A4C6*/ GENx___x___x___ ,
 /*A4C7*/ GENx___x___x___ ,
 /*A4C8*/ GENx___x___x___ ,
 /*A4C9*/ GENx___x___x___ ,
 /*A4CA*/ GENx___x___x___ ,
 /*A4CB*/ GENx___x___x___ ,
 /*A4CC*/ GENx___x___x___ ,
 /*A4CD*/ GENx___x___x___ ,
 /*A4CE*/ GENx___x___x___ ,
 /*A4CF*/ GENx___x___x___ ,
 /*A4D0*/ GENx___x___x___ ,
 /*A4D1*/ GENx___x___x___ ,
 /*A4D2*/ GENx___x___x___ ,
 /*A4D3*/ GENx___x___x___ ,
 /*A4D4*/ GENx___x___x___ ,
 /*A4D5*/ GENx___x___x___ ,
 /*A4D6*/ GENx___x___x___ ,
 /*A4D7*/ GENx___x___x___ ,
 /*A4D8*/ GENx___x___x___ ,
 /*A4D9*/ GENx___x___x___ ,
 /*A4DA*/ GENx___x___x___ ,
 /*A4DB*/ GENx___x___x___ ,
 /*A4DC*/ GENx___x___x___ ,
 /*A4DD*/ GENx___x___x___ ,
 /*A4DE*/ GENx___x___x___ ,
 /*A4DF*/ GENx___x___x___ ,
 /*A4E0*/ GENx___x___x___ ,
 /*A4E1*/ GENx___x___x___ ,
 /*A4E2*/ GENx___x___x___ ,
 /*A4E3*/ GENx___x___x___ ,
 /*A4E4*/ GENx___x___x___ ,
 /*A4E5*/ GENx___x___x___ ,
 /*A4E6*/ GENx___x___x___ ,
 /*A4E7*/ GENx___x___x___ ,
 /*A4E8*/ GENx___x___x___ ,
 /*A4E9*/ GENx___x___x___ ,
 /*A4EA*/ GENx___x___x___ ,
 /*A4EB*/ GENx___x___x___ ,
 /*A4EC*/ GENx___x___x___ ,
 /*A4ED*/ GENx___x___x___ ,
 /*A4EE*/ GENx___x___x___ ,
 /*A4EF*/ GENx___x___x___ ,
 /*A4F0*/ GENx___x___x___ ,
 /*A4F1*/ GENx___x___x___ ,
 /*A4F2*/ GENx___x___x___ ,
 /*A4F3*/ GENx___x___x___ ,
 /*A4F4*/ GENx___x___x___ ,
 /*A4F5*/ GENx___x___x___ ,
 /*A4F6*/ GENx___x___x___ ,
 /*A4F7*/ GENx___x___x___ ,
 /*A4F8*/ GENx___x___x___ ,
 /*A4F9*/ GENx___x___x___ ,
 /*A4FA*/ GENx___x___x___ ,
 /*A4FB*/ GENx___x___x___ ,
 /*A4FC*/ GENx___x___x___ ,
 /*A4FD*/ GENx___x___x___ ,
 /*A4FE*/ GENx___x___x___ ,
 /*A4FF*/ GENx___x___x___  };

// #endif /*defined(FEATURE_ESAME)*/

// #if defined(FEATURE_ESAME)

zz_func opcode_a5xx[16][GEN_MAXARCH] = {
 /*A5x0*/ GENx___x___x900 (insert_immediate_high_high,RI,"IIHH"),
 /*A5x1*/ GENx___x___x900 (insert_immediate_high_low,RI,"IIHL"),
 /*A5x2*/ GENx___x___x900 (insert_immediate_low_high,RI,"IILH"),
 /*A5x3*/ GENx___x___x900 (insert_immediate_low_low,RI,"IILL"),
 /*A5x4*/ GENx___x___x900 (and_immediate_high_high,RI,"NIHH"),
 /*A5x5*/ GENx___x___x900 (and_immediate_high_low,RI,"NIHL"),
 /*A5x6*/ GENx___x___x900 (and_immediate_low_high,RI,"NILH"),
 /*A5x7*/ GENx___x___x900 (and_immediate_low_low,RI,"NILL"),
 /*A5x8*/ GENx___x___x900 (or_immediate_high_high,RI,"OIHH"),
 /*A5x9*/ GENx___x___x900 (or_immediate_high_low,RI,"OIHL"),
 /*A5xA*/ GENx___x___x900 (or_immediate_low_high,RI,"OILH"),
 /*A5xB*/ GENx___x___x900 (or_immediate_low_low,RI,"OILL"),
 /*A5xC*/ GENx___x___x900 (load_logical_immediate_high_high,RI,"LLIHH"),
 /*A5xD*/ GENx___x___x900 (load_logical_immediate_high_low,RI,"LLIHL"),
 /*A5xE*/ GENx___x___x900 (load_logical_immediate_low_high,RI,"LLILH"),
 /*A5xF*/ GENx___x___x900 (load_logical_immediate_low_low,RI,"LLILL") } ;

// #endif /*defined(FEATURE_ESAME)*/

zz_func s370_opcode_a7xx[16];
zz_func s390_opcode_a7xx[16];
zz_func z900_opcode_a7xx[16];

zz_func opcode_a7xx[16][GEN_MAXARCH] = {
 /*A7x0*/ GENx___x390x900 (test_under_mask_high,RI,"TMH"),
 /*A7x1*/ GENx___x390x900 (test_under_mask_low,RI,"TML"),
 /*A7x2*/ GENx___x___x900 (test_under_mask_high_high,RI,"TMHH"),
 /*A7x3*/ GENx___x___x900 (test_under_mask_high_low,RI,"TMHL"),
 /*A7x4*/ GENx___x390x900 (branch_relative_on_condition,RI_B,"BRC"),
 /*A7x5*/ GENx___x390x900 (branch_relative_and_save,RI_B,"BRAS"),
 /*A7x6*/ GENx___x390x900 (branch_relative_on_count,RI_B,"BRCT"),
 /*A7x7*/ GENx___x___x900 (branch_relative_on_count_long,RI_B,"BRCTG"),
 /*A7x8*/ GENx___x390x900 (load_halfword_immediate,RI,"LHI"),
 /*A7x9*/ GENx___x___x900 (load_long_halfword_immediate,RI,"LGHI"),
 /*A7xA*/ GENx___x390x900 (add_halfword_immediate,RI,"AHI"),
 /*A7xB*/ GENx___x___x900 (add_long_halfword_immediate,RI,"AGHI"),
 /*A7xC*/ GENx___x390x900 (multiply_halfword_immediate,RI,"MHI"),
 /*A7xD*/ GENx___x___x900 (multiply_long_halfword_immediate,RI,"MGHI"),
 /*A7xE*/ GENx___x390x900 (compare_halfword_immediate,RI,"CHI"),
 /*A7xF*/ GENx___x___x900 (compare_long_halfword_immediate,RI,"CGHI") };


zz_func opcode_b2xx[256][GEN_MAXARCH] = {
 /*B200*/ GENx370x___x___ (connect_channel_set,S,"CONCS"),
 /*B201*/ GENx370x___x___ (disconnect_channel_set,S,"DISCS"),
 /*B202*/ GENx370x390x900 (store_cpu_id,S,"STIDP"),
 /*B203*/ GENx370x___x___ (store_channel_id,S,"STIDC"),
 /*B204*/ GENx370x390x900 (set_clock,S,"SCK"),
 /*B205*/ GENx370x390x900 (store_clock,S,"STCK"),
 /*B206*/ GENx370x390x900 (set_clock_comparator,S,"SCKC"),
 /*B207*/ GENx370x390x900 (store_clock_comparator,S,"STCKC"),
 /*B208*/ GENx370x390x900 (set_cpu_timer,S,"SPT"),
 /*B209*/ GENx370x390x900 (store_cpu_timer,S,"STPT"),
 /*B20A*/ GENx370x390x900 (set_psw_key_from_address,S,"SPKA"),
 /*B20B*/ GENx370x390x900 (insert_psw_key,S,"IPK"),
 /*B20C*/ GENx___x___x___ ,
 /*B20D*/ GENx370x390x900 (purge_translation_lookaside_buffer,S,"PTLB"),
 /*B20E*/ GENx___x___x___ ,
 /*B20F*/ GENx___x___x___ ,
 /*B210*/ GENx370x390x900 (set_prefix,S,"SPX"),
 /*B211*/ GENx370x390x900 (store_prefix,S,"STPX"),
 /*B212*/ GENx370x390x900 (store_cpu_address,S,"STAP"),
 /*B213*/ GENx370x___x___ (reset_reference_bit,S,"RRB"),
 /*B214*/ GENx___x390x900 (start_interpretive_execution,S,"SIE"),
 /*B215*/ GENx___x___x___ ,
 /*B216*/ GENx___x___x___ ,                                     /*%SETR/SSYN */
 /*B217*/ GENx___x___x___ ,                                   /*%STETR/STSYN */
 /*B218*/ GENx370x390x900 (program_call,S,"PC"),
 /*B219*/ GENx370x390x900 (set_address_space_control_x,S,"SAC"),
 /*B21A*/ GENx___x390x900 (compare_and_form_codeword,S,"CFC"),
 /*B21B*/ GENx___x___x___ ,
 /*B21C*/ GENx___x___x___ ,
 /*B21D*/ GENx___x___x___ ,
 /*B21E*/ GENx___x___x___ ,
 /*B21F*/ GENx___x___x___ ,
 /*B220*/ GENx___x390x900 (service_call,RRE,"SERVC"),
 /*B221*/ GENx370x390x900 (invalidate_page_table_entry,RRE,"IPTE"),
 /*B222*/ GENx370x390x900 (insert_program_mask,RRE,"IPM"),
 /*B223*/ GENx370x390x900 (insert_virtual_storage_key,RRE,"IVSK"),
 /*B224*/ GENx370x390x900 (insert_address_space_control,RRE,"IAC"),
 /*B225*/ GENx370x390x900 (set_secondary_asn,RRE,"SSAR"),
 /*B226*/ GENx370x390x900 (extract_primary_asn,RRE,"EPAR"),
 /*B227*/ GENx370x390x900 (extract_secondary_asn,RRE,"ESAR"),
 /*B228*/ GENx370x390x900 (program_transfer,RRE,"PT"),
 /*B229*/ GENx370x390x900 (insert_storage_key_extended,RRE,"ISKE"),
 /*B22A*/ GENx370x390x900 (reset_reference_bit_extended,RRE,"RRBE"),
 /*B22B*/ GENx370x390x900 (set_storage_key_extended,RRE,"SSKE"),
 /*B22C*/ GENx370x390x900 (test_block,RRE,"TB"),
 /*B22D*/ GENx370x390x900 (divide_float_ext_reg,RRE,"DXR"),
 /*B22E*/ GENx___x390x900 (page_in,RRE,"PGIN"),
 /*B22F*/ GENx___x390x900 (page_out,RRE,"PGOUT"),
 /*B230*/ GENx___x390x900 (clear_subchannel,S,"CSCH"),
 /*B231*/ GENx___x390x900 (halt_subchannel,S,"HSCH"),
 /*B232*/ GENx___x390x900 (modify_subchannel,S,"MSCH"),
 /*B233*/ GENx___x390x900 (start_subchannel,S,"SSCH"),
 /*B234*/ GENx___x390x900 (store_subchannel,S,"STSCH"),
 /*B235*/ GENx___x390x900 (test_subchannel,S,"TSCH"),
 /*B236*/ GENx___x390x900 (test_pending_interruption,S,"TPI"),
 /*B237*/ GENx___x390x900 (set_address_limit,S,"SAL"),
 /*B238*/ GENx___x390x900 (resume_subchannel,S,"RSCH"),
 /*B239*/ GENx___x390x900 (store_channel_report_word,S,"STCRW"),
 /*B23A*/ GENx___x390x900 (store_channel_path_status,S,"STCPS"),
 /*B23B*/ GENx___x390x900 (reset_channel_path,S,"RCHP"),
 /*B23C*/ GENx___x390x900 (set_channel_monitor,S,"SCHM"),
 /*B23D*/ GENx___x___x___ ,                                     /*.STZP      */
 /*B23E*/ GENx___x___x___ ,                                     /*.SZP       */
 /*B23F*/ GENx___x___x___ ,                                     /*.TPZI      */
 /*B240*/ GENx___x390x900 (branch_and_stack,RRE,"BAKR"),
 /*B241*/ GENx___x390x900 (checksum,RRE,"CKSM"),
 /*B242*/ GENx___x___x___ ,                                     /**Add FRR   */
 /*B243*/ GENx___x___x___ ,                                     /*#MA        */
 /*B244*/ GENx___x390x900 (squareroot_float_long_reg,RRE,"SQDR"),
 /*B245*/ GENx___x390x900 (squareroot_float_short_reg,RRE,"SQER"),
 /*B246*/ GENx___x390x900 (store_using_real_address,RRE,"STURA"),
 /*B247*/ GENx___x390x900 (modify_stacked_state,RRE,"MSTA"),
 /*B248*/ GENx___x390x900 (purge_accesslist_lookaside_buffer,RRE,"PALB"),
 /*B249*/ GENx___x390x900 (extract_stacked_registers,RRE,"EREG"),
 /*B24A*/ GENx___x390x900 (extract_stacked_state,RRE,"ESTA"),
 /*B24B*/ GENx___x390x900 (load_using_real_address,RRE,"LURA"),
 /*B24C*/ GENx___x390x900 (test_access,RRE,"TAR"),
 /*B24D*/ GENx___x390x900 (copy_access,RRE,"CPYA"),
 /*B24E*/ GENx___x390x900 (set_access_register,RRE,"SAR"),
 /*B24F*/ GENx___x390x900 (extract_access_register,RRE,"EAR"),
 /*B250*/ GENx___x390x900 (compare_and_swap_and_purge,RRE,"CSP"),
 /*B251*/ GENx___x___x___ ,
 /*B252*/ GENx___x390x900 (multiply_single_register,RRE,"MSR"),
 /*B253*/ GENx___x___x___ ,
 /*B254*/ GENx___x390x900 (move_page,RRE,"MVPG"),
 /*B255*/ GENx___x390x900 (move_string,RRE,"MVST"),
 /*B256*/ GENx___x___x___ ,
 /*B257*/ GENx___x390x900 (compare_until_substring_equal,RRE,"CUSE"),
 /*B258*/ GENx___x390x900 (branch_in_subspace_group,RRE,"BSG"),
 /*B259*/ GENx___x390x900 (invalidate_expanded_storage_block_entry,RRE,"IESBE"),
 /*B25A*/ GENx___x390x900 (branch_and_set_authority,RRE,"BSA"),
 /*B25B*/ GENx___x___x___ ,                                     /*%PGXIN     */
 /*B25C*/ GENx___x___x___ ,                                     /*%PGXOUT    */
 /*B25D*/ GENx___x390x900 (compare_logical_string,RRE,"CLST"),
 /*B25E*/ GENx___x390x900 (search_string,RRE,"SRST"),
 /*B25F*/ GENx___x390x900 (channel_subsystem_call,RRE,"CHSC"),
 /*B260*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B261*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B262*/ GENx___x390x900 (lock_page,RRE,"LKPG"),
 /*B263*/ GENx___x390x900 (compression_call,RRE,"CMPSC"),
 /*B264*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B265*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B266*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B267*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B268*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B269*/ GENx___x___x___ ,                                     /* Crypto    */
 /*B26A*/ GENx___x___x___ ,                                     /* Crypto    */
 /*B26B*/ GENx___x___x___ ,                                     /* Crypto    */
 /*B26C*/ GENx___x___x___ ,                                     /* Crypto    */
 /*B26D*/ GENx___x___x___ ,                                     /* Crypto    */
 /*B26E*/ GENx___x___x___ ,                                     /* Crypto    */
 /*B26F*/ GENx___x___x___ ,                                     /* Crypto    */
 /*B270*/ GENx___x___x___ ,                                     /*%SPCS      */
 /*B271*/ GENx___x___x___ ,                                     /*%STPCS     */
 /*B272*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B273*/ GENx___x___x___ ,                                     /*%SIGA      */
 /*B274*/ GENx___x___x___ ,
 /*B275*/ GENx___x___x___ ,
 /*B276*/ GENx___x390x900 (cancel_subchannel,S,"XSCH"),
 /*B277*/ GENx___x390x900 (resume_program,S,"RP"),
 /*B278*/ GENx___x390x900 (store_clock_extended,S,"STCKE"),
 /*B279*/ GENx___x390x900 (set_address_space_control_x,S,"SACF"),
 /*B27A*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B27B*/ GENx___x___x___ ,                                     /* TFF/Sysplx*/
 /*B27C*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B27D*/ GENx370x390x900 (store_system_information,S,"STSI"),
 /*B27E*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B27F*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B280*/ GENx___x___x___ ,                                     /*#LN L      */
 /*B281*/ GENx___x___x___ ,                                     /*#LN S      */
 /*B282*/ GENx___x___x___ ,                                     /*#EXP L     */
 /*B283*/ GENx___x___x___ ,                                     /*#EXP S     */
 /*B284*/ GENx___x___x___ ,                                     /*#LOG L     */
 /*B285*/ GENx___x___x___ ,                                     /*#LOG S     */
 /*B286*/ GENx___x___x___ ,                                     /*#POWER L   */
 /*B287*/ GENx___x___x___ ,                                     /*#POWER S   */
 /*B288*/ GENx___x___x___ ,                                     /*#SIN L     */
 /*B289*/ GENx___x___x___ ,                                     /*#SIN S     */
 /*B28A*/ GENx___x___x___ ,                                     /*#COS L     */
 /*B28B*/ GENx___x___x___ ,                                     /*#COS S     */
 /*B28C*/ GENx___x___x___ ,
 /*B28D*/ GENx___x___x___ ,
 /*B28E*/ GENx___x___x___ ,
 /*B28F*/ GENx___x___x___ ,
 /*B290*/ GENx___x___x___ ,
 /*B291*/ GENx___x___x___ ,
 /*B292*/ GENx___x___x___ ,
 /*B293*/ GENx___x___x___ ,
 /*B294*/ GENx___x___x___ ,                                     /*#ARCTAN L  */
 /*B295*/ GENx___x___x___ ,                                     /*#ARCTAN S  */
 /*B296*/ GENx___x___x___ ,
 /*B297*/ GENx___x___x___ ,
 /*B298*/ GENx___x___x___ ,
 /*B299*/ GENx___x390x900 (set_rounding_mode,S,"SRNM"),
 /*B29A*/ GENx___x___x___ ,
 /*B29B*/ GENx___x___x___ ,
 /*B29C*/ GENx___x390x900 (store_fpc,S,"STFPC"),
 /*B29D*/ GENx___x390x900 (load_fpc,S,"LFPC"),
 /*B29E*/ GENx___x___x___ ,
 /*B29F*/ GENx___x___x___ ,
 /*B2A0*/ GENx___x___x___ ,
 /*B2A1*/ GENx___x___x___ ,
 /*B2A2*/ GENx___x___x___ ,
 /*B2A3*/ GENx___x___x___ ,
 /*B2A4*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B2A5*/ GENx___x390x900 (translate_extended,RRE,"TRE"),
 /*B2A6*/ GENx___x390x900 (convert_unicode_to_utf8,RRE,"CUUTF"),
 /*B2A7*/ GENx___x390x900 (convert_utf8_to_unicode,RRE,"CUTFU"),
 /*B2A8*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B2A9*/ GENx___x___x___ ,
 /*B2AA*/ GENx___x___x___ ,
 /*B2AB*/ GENx___x___x___ ,
 /*B2AC*/ GENx___x___x___ ,
 /*B2AD*/ GENx___x___x___ ,
 /*B2AE*/ GENx___x___x___ ,
 /*B2AF*/ GENx___x___x___ ,
 /*B2B0*/ GENx___x___x___ ,                                     /*!SARCH     */
 /*B2B1*/ GENx___x390x900 (store_facilities_list,S,"STFL"),
 /*B2B2*/ GENx___x___x900 (load_program_status_word_extended,S,"LPSWE"),
 /*B2B3*/ GENx___x___x___ ,
 /*B2B4*/ GENx___x___x___ ,
 /*B2B5*/ GENx___x___x___ ,
 /*B2B6*/ GENx___x___x___ ,
 /*B2B7*/ GENx___x___x___ ,
 /*B2B8*/ GENx___x___x___ ,
 /*B2B9*/ GENx___x___x___ ,
 /*B2BA*/ GENx___x___x___ ,
 /*B2BB*/ GENx___x___x___ ,
 /*B2BC*/ GENx___x___x___ ,
 /*B2BD*/ GENx___x___x___ ,
 /*B2BE*/ GENx___x___x___ ,
 /*B2BF*/ GENx___x___x___ ,
 /*B2C0*/ GENx___x___x___ ,                                     /*$ADRN      */
 /*B2C1*/ GENx___x___x___ ,                                     /*$AERN      */
 /*B2C2*/ GENx___x___x___ ,                                     /*$SDRN      */
 /*B2C3*/ GENx___x___x___ ,                                     /*$SERN      */
 /*B2C4*/ GENx___x___x___ ,                                     /*$MDRN      */
 /*B2C5*/ GENx___x___x___ ,                                     /*$MERN      */
 /*B2C6*/ GENx___x___x___ ,                                     /*$DDRN      */
 /*B2C7*/ GENx___x___x___ ,                                     /*$DERN      */
 /*B2C8*/ GENx___x___x___ ,                                     /*$LERN      */
 /*B2C9*/ GENx___x___x___ ,
 /*B2CA*/ GENx___x___x___ ,
 /*B2CB*/ GENx___x___x___ ,
 /*B2CC*/ GENx___x___x___ ,
 /*B2CD*/ GENx___x___x___ ,
 /*B2CE*/ GENx___x___x___ ,
 /*B2CF*/ GENx___x___x___ ,
 /*B2D0*/ GENx___x___x___ ,                                     /*$AACDR     */
 /*B2D1*/ GENx___x___x___ ,                                     /*$AACER     */
 /*B2D2*/ GENx___x___x___ ,                                     /*$SACDR     */
 /*B2D3*/ GENx___x___x___ ,                                     /*$SACER     */
 /*B2D4*/ GENx___x___x___ ,                                     /*$MACD      */
 /*B2D5*/ GENx___x___x___ ,
 /*B2D6*/ GENx___x___x___ ,                                     /*$RACD      */
 /*B2D7*/ GENx___x___x___ ,                                     /*$RACE      */
 /*B2D8*/ GENx___x___x___ ,                                     /*$AACAC     */
 /*B2D9*/ GENx___x___x___ ,                                     /*$SACAC     */
 /*B2DA*/ GENx___x___x___ ,                                     /*$CLAC      */
 /*B2DB*/ GENx___x___x___ ,
 /*B2DC*/ GENx___x___x___ ,
 /*B2DD*/ GENx___x___x___ ,
 /*B2DE*/ GENx___x___x___ ,
 /*B2DF*/ GENx___x___x___ ,
 /*B2E0*/ GENx___x___x___ ,
 /*B2E1*/ GENx___x___x___ ,
 /*B2E2*/ GENx___x___x___ ,
 /*B2E3*/ GENx___x___x___ ,
 /*B2E4*/ GENx___x___x___ ,
 /*B2E5*/ GENx___x___x___ ,
 /*B2E6*/ GENx___x___x___ ,
 /*B2E7*/ GENx___x___x___ ,
 /*B2E8*/ GENx___x___x___ ,
 /*B2E9*/ GENx___x___x___ ,
 /*B2EA*/ GENx___x___x___ ,
 /*B2EB*/ GENx___x___x___ ,
 /*B2EC*/ GENx___x___x___ ,
 /*B2ED*/ GENx___x___x___ ,
 /*B2EE*/ GENx___x___x___ ,
 /*B2EF*/ GENx___x___x___ ,
 /*B2F0*/ GENx370x390x900 (inter_user_communication_vehicle,S,"IUCV"),
 /*B2F1*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B2F2*/ GENx___x___x___ ,
 /*B2F3*/ GENx___x___x___ ,
 /*B2F4*/ GENx___x___x___ ,
 /*B2F5*/ GENx___x___x___ ,
 /*B2F6*/ GENx___x___x___ ,                                     /* Sysplex   */
 /*B2F7*/ GENx___x___x___ ,
 /*B2F8*/ GENx___x___x___ ,
 /*B2F9*/ GENx___x___x___ ,
 /*B2FA*/ GENx___x___x___ ,
 /*B2FB*/ GENx___x___x___ ,
 /*B2FC*/ GENx___x___x___ ,
 /*B2FD*/ GENx___x___x___ ,
 /*B2FE*/ GENx___x___x___ ,
 /*B2FF*/ GENx___x390x900 (trap4,S,"TRAP") };


// #if defined(FEATURE_BASIC_FP_EXTENSIONS)

zz_func opcode_b3xx[256][GEN_MAXARCH] = {
 /*B300*/ GENx___x390x900 (load_positive_bfp_short_reg,RRE,"LPEBR"),
 /*B301*/ GENx___x390x900 (load_negative_bfp_short_reg,RRE,"LNEBR"),
 /*B302*/ GENx___x390x900 (load_and_test_bfp_short_reg,RRE,"LTEBR"),
 /*B303*/ GENx___x390x900 (load_complement_bfp_short_reg,RRE,"LCEBR"),
 /*B304*/ GENx___x390x900 (loadlength_bfp_short_to_long_reg,RRE,"LDEBR"),
 /*B305*/ GENx___x___x___ ,
 /*B306*/ GENx___x___x___ ,
 /*B307*/ GENx___x___x___ ,
 /*B308*/ GENx___x390x900 (compare_and_signal_bfp_short_reg,RRE,"KEBR"),
 /*B309*/ GENx___x390x900 (compare_bfp_short_reg,RRE,"CEBR"),
 /*B30A*/ GENx___x390x900 (add_bfp_short_reg,RRE,"AEBR"),
 /*B30B*/ GENx___x390x900 (subtract_bfp_short_reg,RRE,"SEBR"),
 /*B30C*/ GENx___x___x___ ,
 /*B30D*/ GENx___x390x900 (divide_bfp_short_reg,RRE,"DEBR"),
 /*B30E*/ GENx___x___x___ ,
 /*B30F*/ GENx___x___x___ ,
 /*B310*/ GENx___x390x900 (load_positive_bfp_long_reg,RRE,"LPDBR"),
 /*B311*/ GENx___x390x900 (load_negative_bfp_long_reg,RRE,"LNDBR"),
 /*B312*/ GENx___x390x900 (load_and_test_bfp_long_reg,RRE,"LTDBR"),
 /*B313*/ GENx___x390x900 (load_complement_bfp_long_reg,RRE,"LCDBR"),
 /*B314*/ GENx___x390x900 (squareroot_bfp_short_reg,RRE,"SQEBR"),
 /*B315*/ GENx___x390x900 (squareroot_bfp_long_reg,RRE,"SQDBR"),
 /*B316*/ GENx___x390x900 (squareroot_bfp_ext_reg,RRE,"SQXBR"),
 /*B317*/ GENx___x390x900 (multiply_bfp_short_reg,RRE,"MEEBR"),
 /*B318*/ GENx___x390x900 (compare_and_signal_bfp_long_reg,RRE,"KDBR"),
 /*B319*/ GENx___x390x900 (compare_bfp_long_reg,RRE,"CDBR"),
 /*B31A*/ GENx___x390x900 (add_bfp_long_reg,RRE,"ADBR"),
 /*B31B*/ GENx___x390x900 (subtract_bfp_long_reg,RRE,"SDBR"),
 /*B31C*/ GENx___x390x900 (multiply_bfp_long_reg,RRE,"MDBR"),
 /*B31D*/ GENx___x390x900 (divide_bfp_long_reg,RRE,"DDBR"),
 /*B31E*/ GENx___x___x___ ,
 /*B31F*/ GENx___x___x___ ,
 /*B320*/ GENx___x___x___ ,
 /*B321*/ GENx___x___x___ ,
 /*B322*/ GENx___x___x___ ,
 /*B323*/ GENx___x___x___ ,
 /*B324*/ GENx___x390x900 (loadlength_float_short_to_long_reg,RRE,"LDER"),
 /*B325*/ GENx___x390x900 (loadlength_float_long_to_ext_reg,RRE,"LXDR"),
 /*B326*/ GENx___x390x900 (loadlength_float_short_to_ext_reg,RRE,"LXER"),
 /*B327*/ GENx___x___x___ ,
 /*B328*/ GENx___x___x___ ,
 /*B329*/ GENx___x___x___ ,
 /*B32A*/ GENx___x___x___ ,
 /*B32B*/ GENx___x___x___ ,
 /*B32C*/ GENx___x___x___ ,
 /*B32D*/ GENx___x___x___ ,
 /*B32E*/ GENx___x___x___ ,
 /*B32F*/ GENx___x___x___ ,
 /*B330*/ GENx___x___x___ ,
 /*B331*/ GENx___x___x___ ,
 /*B332*/ GENx___x___x___ ,
 /*B333*/ GENx___x___x___ ,
 /*B334*/ GENx___x___x___ ,
 /*B335*/ GENx___x___x___ ,
 /*B336*/ GENx___x390x900 (squareroot_float_ext_reg,RRE,"SQXR"),
 /*B337*/ GENx___x390x900 (multiply_float_short_reg,RRE,"MEER"),
 /*B338*/ GENx___x___x___ ,
 /*B339*/ GENx___x___x___ ,
 /*B33A*/ GENx___x___x___ ,
 /*B33B*/ GENx___x___x___ ,
 /*B33C*/ GENx___x___x___ ,
 /*B33D*/ GENx___x___x___ ,
 /*B33E*/ GENx___x___x___ ,
 /*B33F*/ GENx___x___x___ ,
 /*B340*/ GENx___x390x900 (load_positive_bfp_ext_reg,RRE,"LPXBR"),
 /*B341*/ GENx___x390x900 (load_negative_bfp_ext_reg,RRE,"LNXBR"),
 /*B342*/ GENx___x390x900 (load_and_test_bfp_ext_reg,RRE,"LTXBR"),
 /*B343*/ GENx___x390x900 (load_complement_bfp_ext_reg,RRE,"LCXBR"),
 /*B344*/ GENx___x390x900 (round_bfp_long_to_short_reg,RRE,"LEDBR"),
 /*B345*/ GENx___x___x___ ,
 /*B346*/ GENx___x___x___ ,
 /*B347*/ GENx___x___x___ ,
 /*B348*/ GENx___x390x900 (compare_and_signal_bfp_ext_reg,RRE,"KXBR"),
 /*B349*/ GENx___x390x900 (compare_bfp_ext_reg,RRE,"CXBR"),
 /*B34A*/ GENx___x390x900 (add_bfp_ext_reg,RRE,"AXBR"),
 /*B34B*/ GENx___x390x900 (subtract_bfp_ext_reg,RRE,"SXBR"),
 /*B34C*/ GENx___x390x900 (multiply_bfp_ext_reg,RRE,"MXBR"),
 /*B34D*/ GENx___x390x900 (divide_bfp_ext_reg,RRE,"DXBR"),
 /*B34E*/ GENx___x___x___ ,
 /*B34F*/ GENx___x___x___ ,
 /*B350*/ GENx___x390x900 (convert_float_long_to_bfp_short_reg,RRF_M,"TBEDR"),
 /*B351*/ GENx___x390x900 (convert_float_long_to_bfp_long_reg,RRF_M,"TBDR"),
 /*B352*/ GENx___x___x___ ,
 /*B353*/ GENx___x___x___ ,
 /*B354*/ GENx___x___x___ ,
 /*B355*/ GENx___x___x___ ,
 /*B356*/ GENx___x___x___ ,
 /*B357*/ GENx___x___x___ ,
 /*B358*/ GENx___x390x900 (convert_bfp_short_to_float_long_reg,RRE,"THDER"),
 /*B359*/ GENx___x390x900 (convert_bfp_long_to_float_long_reg,RRE,"THDR"),
 /*B35A*/ GENx___x___x___ ,
 /*B35B*/ GENx___x___x___ ,
 /*B35C*/ GENx___x___x___ ,
 /*B35D*/ GENx___x___x___ ,
 /*B35E*/ GENx___x___x___ ,
 /*B35F*/ GENx___x___x___ ,
 /*B360*/ GENx___x390x900 (load_positive_float_ext_reg,RRE,"LPXR"),
 /*B361*/ GENx___x390x900 (load_negative_float_ext_reg,RRE,"LNXR"),
 /*B362*/ GENx___x390x900 (load_and_test_float_ext_reg,RRE,"LTXR"),
 /*B363*/ GENx___x390x900 (load_complement_float_ext_reg,RRE,"LCXR"),
 /*B364*/ GENx___x___x___ ,
 /*B365*/ GENx___x390x900 (load_float_ext_reg,RRE,"LXR"),
 /*B366*/ GENx___x390x900 (round_float_ext_to_short_reg,RRE,"LEXR"),
 /*B367*/ GENx___x390x900 (load_fp_int_float_ext_reg,RRE,"FIXR"),
 /*B368*/ GENx___x___x___ ,
 /*B369*/ GENx___x390x900 (compare_float_ext_reg,RRE,"CXR"),
 /*B36A*/ GENx___x___x___ ,
 /*B36B*/ GENx___x___x___ ,
 /*B36C*/ GENx___x___x___ ,
 /*B36D*/ GENx___x___x___ ,
 /*B36E*/ GENx___x___x___ ,
 /*B36F*/ GENx___x___x___ ,
 /*B370*/ GENx___x___x___ ,
 /*B371*/ GENx___x___x___ ,
 /*B372*/ GENx___x___x___ ,
 /*B373*/ GENx___x___x___ ,
 /*B374*/ GENx___x390x900 (load_zero_float_short_reg,RRE,"LZER"),
 /*B375*/ GENx___x390x900 (load_zero_float_long_reg,RRE,"LZDR"),
 /*B376*/ GENx___x390x900 (load_zero_float_ext_reg,RRE,"LZXR"),
 /*B377*/ GENx___x390x900 (load_fp_int_float_short_reg,RRE,"FIER"),
 /*B378*/ GENx___x___x___ ,
 /*B379*/ GENx___x___x___ ,
 /*B37A*/ GENx___x___x___ ,
 /*B37B*/ GENx___x___x___ ,
 /*B37C*/ GENx___x___x___ ,
 /*B37D*/ GENx___x___x___ ,
 /*B37E*/ GENx___x___x___ ,
 /*B37F*/ GENx___x390x900 (load_fp_int_float_long_reg,RRE,"FIDR"),
 /*B380*/ GENx___x___x___ ,
 /*B381*/ GENx___x___x___ ,
 /*B382*/ GENx___x___x___ ,
 /*B383*/ GENx___x___x___ ,
 /*B384*/ GENx___x390x900 (set_fpc,RRE,"SFPC"),
 /*B385*/ GENx___x___x___ ,
 /*B386*/ GENx___x___x___ ,
 /*B387*/ GENx___x___x___ ,
 /*B388*/ GENx___x___x___ ,
 /*B389*/ GENx___x___x___ ,
 /*B38A*/ GENx___x___x___ ,
 /*B38B*/ GENx___x___x___ ,
 /*B38C*/ GENx___x390x900 (extract_fpc,RRE,"EFPC"),
 /*B38D*/ GENx___x___x___ ,
 /*B38E*/ GENx___x___x___ ,
 /*B38F*/ GENx___x___x___ ,
 /*B390*/ GENx___x___x___ ,
 /*B391*/ GENx___x___x___ ,
 /*B392*/ GENx___x___x___ ,
 /*B393*/ GENx___x___x___ ,
 /*B394*/ GENx___x390x900 (convert_fix32_to_bfp_short_reg,RRE,"CEFBR"),
 /*B395*/ GENx___x390x900 (convert_fix32_to_bfp_long_reg,RRE,"CDFBR"),
 /*B396*/ GENx___x___x___ ,
 /*B397*/ GENx___x___x___ ,
 /*B398*/ GENx___x390x900 (convert_bfp_short_to_fix32_reg,RRF_M,"CFEBR"),
 /*B399*/ GENx___x390x900 (convert_bfp_long_to_fix32_reg,RRF_M,"CFDBR"),
 /*B39A*/ GENx___x___x___ ,
 /*B39B*/ GENx___x___x___ ,
 /*B39C*/ GENx___x___x___ ,
 /*B39D*/ GENx___x___x___ ,
 /*B39E*/ GENx___x___x___ ,
 /*B39F*/ GENx___x___x___ ,
 /*B3A0*/ GENx___x___x___ ,
 /*B3A1*/ GENx___x___x___ ,
 /*B3A2*/ GENx___x___x___ ,
 /*B3A3*/ GENx___x___x___ ,
 /*B3A4*/ GENx___x___x900 (convert_fix64_to_bfp_short_reg,RRE,"CEGBR"),
 /*B3A5*/ GENx___x___x900 (convert_fix64_to_bfp_long_reg,RRE,"CDGBR"),
 /*B3A6*/ GENx___x___x___ ,                                     /*!CXGBR     */
 /*B3A7*/ GENx___x___x___ ,
 /*B3A8*/ GENx___x___x900 (convert_bfp_short_to_fix64_reg,RRF_M,"CGEBR"),
 /*B3A9*/ GENx___x___x900 (convert_bfp_long_to_fix64_reg,RRF_M,"CGDBR"),
 /*B3AA*/ GENx___x___x___ ,
 /*B3AB*/ GENx___x___x___ ,
 /*B3AC*/ GENx___x___x___ ,
 /*B3AD*/ GENx___x___x___ ,
 /*B3AE*/ GENx___x___x___ ,
 /*B3AF*/ GENx___x___x___ ,
 /*B3B0*/ GENx___x___x___ ,
 /*B3B1*/ GENx___x___x___ ,
 /*B3B2*/ GENx___x___x___ ,
 /*B3B3*/ GENx___x___x___ ,
 /*B3B4*/ GENx___x390x900 (convert_fixed_to_float_short_reg,RRE,"CEFR"),
 /*B3B5*/ GENx___x390x900 (convert_fixed_to_float_long_reg,RRE,"CDFR"),
 /*B3B6*/ GENx___x390x900 (convert_fixed_to_float_ext_reg,RRE,"CXFR"),
 /*B3B7*/ GENx___x___x___ ,
 /*B3B8*/ GENx___x390x900 (convert_float_short_to_fixed_reg,RRF_M,"CFER"),
 /*B3B3*/ GENx___x390x900 (convert_float_long_to_fixed_reg,RRF_M,"CFDR"),
 /*B3BA*/ GENx___x390x900 (convert_float_ext_to_fixed_reg,RRF_M,"CFXR"),
 /*B3BB*/ GENx___x___x___ ,
 /*B3BC*/ GENx___x___x___ ,
 /*B3BD*/ GENx___x___x___ ,
 /*B3BE*/ GENx___x___x___ ,
 /*B3BF*/ GENx___x___x___ ,
 /*B3C0*/ GENx___x___x___ ,
 /*B3C1*/ GENx___x___x___ ,
 /*B3C2*/ GENx___x___x___ ,
 /*B3C3*/ GENx___x___x___ ,
 /*B3C4*/ GENx___x___x___ ,                                     /*!CEGR      */
 /*B3C5*/ GENx___x___x___ ,                                     /*!CDGR      */
 /*B3C6*/ GENx___x___x___ ,                                     /*!CXGR      */
 /*B3C7*/ GENx___x___x___ ,
 /*B3C8*/ GENx___x___x___ ,                                     /*!CGER      */
 /*B3C9*/ GENx___x___x___ ,                                     /*!CGDR      */
 /*B3CA*/ GENx___x___x___ ,                                     /*!CGXR      */
 /*B3CB*/ GENx___x___x___ ,
 /*B3CC*/ GENx___x___x___ ,
 /*B3CD*/ GENx___x___x___ ,
 /*B3CE*/ GENx___x___x___ ,
 /*B3CF*/ GENx___x___x___ ,
 /*B3D0*/ GENx___x___x___ ,
 /*B3D1*/ GENx___x___x___ ,
 /*B3D2*/ GENx___x___x___ ,
 /*B3D3*/ GENx___x___x___ ,
 /*B3D4*/ GENx___x___x___ ,
 /*B3D5*/ GENx___x___x___ ,
 /*B3D6*/ GENx___x___x___ ,
 /*B3D7*/ GENx___x___x___ ,
 /*B3D8*/ GENx___x___x___ ,
 /*B3D9*/ GENx___x___x___ ,
 /*B3DA*/ GENx___x___x___ ,
 /*B3DB*/ GENx___x___x___ ,
 /*B3DC*/ GENx___x___x___ ,
 /*B3DD*/ GENx___x___x___ ,
 /*B3DE*/ GENx___x___x___ ,
 /*B3DF*/ GENx___x___x___ ,
 /*B3E0*/ GENx___x___x___ ,
 /*B3E1*/ GENx___x___x___ ,
 /*B3E2*/ GENx___x___x___ ,
 /*B3E3*/ GENx___x___x___ ,
 /*B3E4*/ GENx___x___x___ ,
 /*B3E5*/ GENx___x___x___ ,
 /*B3E6*/ GENx___x___x___ ,
 /*B3E7*/ GENx___x___x___ ,
 /*B3E8*/ GENx___x___x___ ,
 /*B3E9*/ GENx___x___x___ ,
 /*B3EA*/ GENx___x___x___ ,
 /*B3EB*/ GENx___x___x___ ,
 /*B3EC*/ GENx___x___x___ ,
 /*B3ED*/ GENx___x___x___ ,
 /*B3EE*/ GENx___x___x___ ,
 /*B3EF*/ GENx___x___x___ ,
 /*B3F0*/ GENx___x___x___ ,
 /*B3F1*/ GENx___x___x___ ,
 /*B3F2*/ GENx___x___x___ ,
 /*B3F3*/ GENx___x___x___ ,
 /*B3F4*/ GENx___x___x___ ,
 /*B3F5*/ GENx___x___x___ ,
 /*B3F6*/ GENx___x___x___ ,
 /*B3F7*/ GENx___x___x___ ,
 /*B3F8*/ GENx___x___x___ ,
 /*B3F9*/ GENx___x___x___ ,
 /*B3FA*/ GENx___x___x___ ,
 /*B3FB*/ GENx___x___x___ ,
 /*B3FC*/ GENx___x___x___ ,
 /*B3FD*/ GENx___x___x___ ,
 /*B3FE*/ GENx___x___x___ ,
 /*B3FF*/ GENx___x___x___  };

// #endif /*defined(FEATURE_BASIC_FP_EXTENSIONS)*/

// #if defined(FEATURE_ESAME)

zz_func opcode_b9xx[256][GEN_MAXARCH] = {
 /*B900*/ GENx___x___x900 (load_positive_long_register,RRE,"LPGR"),
 /*B901*/ GENx___x___x900 (load_negative_long_register,RRE,"LNGR"),
 /*B902*/ GENx___x___x900 (load_and_test_long_register,RRE,"LTGR"),
 /*B903*/ GENx___x___x900 (load_complement_long_register,RRE,"LCGR"),
 /*B904*/ GENx___x___x900 (load_long_register,RRE,"LGR"),
 /*B905*/ GENx___x___x900 (load_using_real_address_long,RRE,"LURAG"),
 /*B906*/ GENx___x___x___ ,
 /*B907*/ GENx___x___x___ ,
 /*B908*/ GENx___x___x900 (add_long_register,RRE,"AGR"),
 /*B909*/ GENx___x___x900 (subtract_long_register,RRE,"SGR"),
 /*B90A*/ GENx___x___x900 (add_logical_long_register,RRE,"ALGR"),
 /*B90B*/ GENx___x___x900 (subtract_logical_long_register,RRE,"SLGR"),
 /*B90C*/ GENx___x___x900 (multiply_single_long_register,RRE,"MSGR"),
 /*B90D*/ GENx___x___x900 (divide_single_long_register,RRE,"DSGR"),
 /*B90E*/ GENx___x___x900 (extract_stacked_registers_long,RRE,"EREGG"),
 /*B90F*/ GENx___x___x900 (load_reversed_long_register,RRE,"LRVGR"),
 /*B910*/ GENx___x___x900 (load_positive_long_fullword_register,RRE,"LPGFR"),
 /*B911*/ GENx___x___x900 (load_negative_long_fullword_register,RRE,"LNGFR"),
 /*B912*/ GENx___x___x900 (load_and_test_long_fullword_register,RRE,"LTGFR"),
 /*B913*/ GENx___x___x900 (load_complement_long_fullword_register,RRE,"LCGFR"),
 /*B914*/ GENx___x___x900 (load_long_fullword_register,RRE,"LGFR"),
 /*B915*/ GENx___x___x___ ,
 /*B916*/ GENx___x___x900 (load_logical_long_fullword_register,RRE,"LLGFR"),
 /*B917*/ GENx___x___x900 (load_logical_long_thirtyone_register,RRE,"LLGTR"),
 /*B918*/ GENx___x___x900 (add_long_fullword_register,RRE,"AGFR"),
 /*B919*/ GENx___x___x900 (subtract_long_fullword_register,RRE,"SGFR"),
 /*B91A*/ GENx___x___x900 (add_logical_long_fullword_register,RRE,"ALGFR"),
 /*B91B*/ GENx___x___x900 (subtract_logical_long_fullword_register,RRE,"SLGFR"),
 /*B91C*/ GENx___x___x900 (multiply_single_long_fullword_register,RRE,"MSGFR"),
 /*B91D*/ GENx___x___x900 (divide_single_long_fullword_register,RRE,"DSGFR"),
 /*B91E*/ GENx___x___x___ ,
 /*B91F*/ GENx___x390x900 (load_reversed_register,RRE,"LRVR"),
 /*B920*/ GENx___x___x900 (compare_long_register,RRE,"CGR"),
 /*B921*/ GENx___x___x900 (compare_logical_long_register,RRE,"CLGR"),
 /*B922*/ GENx___x___x___ ,
 /*B923*/ GENx___x___x___ ,
 /*B924*/ GENx___x___x___ ,
 /*B925*/ GENx___x___x900 (store_using_real_address_long,RRE,"STURG"),
 /*B926*/ GENx___x___x___ ,
 /*B927*/ GENx___x___x___ ,
 /*B928*/ GENx___x___x___ ,
 /*B929*/ GENx___x___x___ ,
 /*B92A*/ GENx___x___x___ ,
 /*B92B*/ GENx___x___x___ ,
 /*B92C*/ GENx___x___x___ ,
 /*B92D*/ GENx___x___x___ ,
 /*B92E*/ GENx___x___x___ ,
 /*B92F*/ GENx___x___x___ ,
 /*B930*/ GENx___x___x900 (compare_long_fullword_register,RRE,"CGFR"),
 /*B931*/ GENx___x___x900 (compare_logical_long_fullword_register,RRE,"CLGFR"),
 /*B932*/ GENx___x___x___ ,
 /*B933*/ GENx___x___x___ ,
 /*B934*/ GENx___x___x___ ,
 /*B935*/ GENx___x___x___ ,
 /*B936*/ GENx___x___x___ ,
 /*B937*/ GENx___x___x___ ,
 /*B938*/ GENx___x___x___ ,
 /*B939*/ GENx___x___x___ ,
 /*B93A*/ GENx___x___x___ ,
 /*B93B*/ GENx___x___x___ ,
 /*B93C*/ GENx___x___x___ ,
 /*B93D*/ GENx___x___x___ ,
 /*B93E*/ GENx___x___x___ ,
 /*B93F*/ GENx___x___x___ ,
 /*B940*/ GENx___x___x___ ,
 /*B941*/ GENx___x___x___ ,
 /*B942*/ GENx___x___x___ ,
 /*B943*/ GENx___x___x___ ,
 /*B944*/ GENx___x___x___ ,
 /*B945*/ GENx___x___x___ ,
 /*B946*/ GENx___x___x900 (branch_on_count_long_register,RRE,"BCTGR"),
 /*B947*/ GENx___x___x___ ,
 /*B948*/ GENx___x___x___ ,
 /*B949*/ GENx___x___x___ ,
 /*B94A*/ GENx___x___x___ ,
 /*B94B*/ GENx___x___x___ ,
 /*B94C*/ GENx___x___x___ ,
 /*B94D*/ GENx___x___x___ ,
 /*B94E*/ GENx___x___x___ ,
 /*B94F*/ GENx___x___x___ ,
 /*B950*/ GENx___x___x___ ,
 /*B951*/ GENx___x___x___ ,
 /*B952*/ GENx___x___x___ ,
 /*B953*/ GENx___x___x___ ,
 /*B954*/ GENx___x___x___ ,
 /*B955*/ GENx___x___x___ ,
 /*B956*/ GENx___x___x___ ,
 /*B957*/ GENx___x___x___ ,
 /*B958*/ GENx___x___x___ ,
 /*B959*/ GENx___x___x___ ,
 /*B95A*/ GENx___x___x___ ,
 /*B95B*/ GENx___x___x___ ,
 /*B95C*/ GENx___x___x___ ,
 /*B95D*/ GENx___x___x___ ,
 /*B95E*/ GENx___x___x___ ,
 /*B95F*/ GENx___x___x___ ,
 /*B960*/ GENx___x___x___ ,
 /*B961*/ GENx___x___x___ ,
 /*B962*/ GENx___x___x___ ,
 /*B963*/ GENx___x___x___ ,
 /*B964*/ GENx___x___x___ ,
 /*B965*/ GENx___x___x___ ,
 /*B966*/ GENx___x___x___ ,
 /*B967*/ GENx___x___x___ ,
 /*B968*/ GENx___x___x___ ,
 /*B969*/ GENx___x___x___ ,
 /*B96A*/ GENx___x___x___ ,
 /*B96B*/ GENx___x___x___ ,
 /*B96C*/ GENx___x___x___ ,
 /*B96D*/ GENx___x___x___ ,
 /*B96E*/ GENx___x___x___ ,
 /*B96F*/ GENx___x___x___ ,
 /*B970*/ GENx___x___x___ ,
 /*B971*/ GENx___x___x___ ,
 /*B972*/ GENx___x___x___ ,
 /*B973*/ GENx___x___x___ ,
 /*B974*/ GENx___x___x___ ,
 /*B975*/ GENx___x___x___ ,
 /*B976*/ GENx___x___x___ ,
 /*B977*/ GENx___x___x___ ,
 /*B978*/ GENx___x___x___ ,
 /*B979*/ GENx___x___x___ ,
 /*B97A*/ GENx___x___x___ ,
 /*B97B*/ GENx___x___x___ ,
 /*B97C*/ GENx___x___x___ ,
 /*B97D*/ GENx___x___x___ ,
 /*B97E*/ GENx___x___x___ ,
 /*B97F*/ GENx___x___x___ ,
 /*B980*/ GENx___x___x900 (and_long_register,RRE,"NGR"),
 /*B981*/ GENx___x___x900 (or_long_register,RRE,"OGR"),
 /*B982*/ GENx___x___x900 (exclusive_or_long_register,RRE,"XGR"),
 /*B983*/ GENx___x___x___ ,
 /*B984*/ GENx___x___x___ ,
 /*B985*/ GENx___x___x___ ,
 /*B986*/ GENx___x___x900 (multiply_logical_long_register,RRE,"MLGR"),
 /*B987*/ GENx___x___x900 (divide_logical_long_register,RRE,"DLGR"),
 /*B988*/ GENx___x___x900 (add_logical_carry_long_register,RRE,"ALCGR"),
 /*B989*/ GENx___x___x900 (subtract_logical_borrow_long_register,RRE,"SLBGR"),
 /*B98A*/ GENx___x___x___ ,
 /*B98B*/ GENx___x___x___ ,
 /*B98C*/ GENx___x___x___ ,
 /*B98D*/ GENx___x390x900 (extract_psw,RRE,"EPSW"),
 /*B98E*/ GENx___x___x___ ,
 /*B98F*/ GENx___x___x___ ,
 /*B990*/ GENx___x___x900 (dummy_instruction,RRE,"TRTT"),
 /*B991*/ GENx___x___x900 (dummy_instruction,RRE,"TRTO"),
 /*B992*/ GENx___x___x900 (dummy_instruction,RRE,"TROT"),
 /*B993*/ GENx___x___x900 (dummy_instruction,RRE,"TROO"),
 /*B994*/ GENx___x___x___ ,
 /*B995*/ GENx___x___x___ ,
 /*B996*/ GENx___x390x900 (multiply_logical_register,RRE,"MLR"),
 /*B997*/ GENx___x390x900 (divide_logical_register,RRE,"DLR"),
 /*B998*/ GENx___x390x900 (add_logical_carry_register,RRE,"ALCR"),
 /*B999*/ GENx___x390x900 (subtract_logical_borrow_register,RRE,"SLBR"),
 /*B99A*/ GENx___x___x___ ,
 /*B99B*/ GENx___x___x___ ,
 /*B99C*/ GENx___x___x___ ,
 /*B99D*/ GENx___x___x900 (extract_and_set_extended_authority,RRE,"ESEA"),
 /*B99E*/ GENx___x___x___ ,
 /*B99F*/ GENx___x___x___ ,
 /*B9A0*/ GENx___x___x___ ,
 /*B9A1*/ GENx___x___x___ ,
 /*B9A2*/ GENx___x___x___ ,
 /*B9A3*/ GENx___x___x___ ,
 /*B9B9*/ GENx___x___x___ ,
 /*B9A5*/ GENx___x___x___ ,
 /*B9A6*/ GENx___x___x___ ,
 /*B9A7*/ GENx___x___x___ ,
 /*B9A8*/ GENx___x___x___ ,
 /*B9A9*/ GENx___x___x___ ,
 /*B9AA*/ GENx___x___x___ ,
 /*B9AB*/ GENx___x___x___ ,
 /*B9AC*/ GENx___x___x___ ,
 /*B9AD*/ GENx___x___x___ ,
 /*B9AE*/ GENx___x___x___ ,
 /*B9AF*/ GENx___x___x___ ,
 /*B9B0*/ GENx___x___x___ ,
 /*B9B1*/ GENx___x___x___ ,
 /*B9B2*/ GENx___x___x___ ,
 /*B9B3*/ GENx___x___x___ ,
 /*B9B4*/ GENx___x___x___ ,
 /*B9B5*/ GENx___x___x___ ,
 /*B9B6*/ GENx___x___x___ ,
 /*B9B7*/ GENx___x___x___ ,
 /*B9B8*/ GENx___x___x___ ,
 /*B9B9*/ GENx___x___x___ ,
 /*B9BA*/ GENx___x___x___ ,
 /*B9BB*/ GENx___x___x___ ,
 /*B9BC*/ GENx___x___x___ ,
 /*B9BD*/ GENx___x___x___ ,
 /*B9BE*/ GENx___x___x___ ,
 /*B9BF*/ GENx___x___x___ ,
 /*B9C0*/ GENx___x___x___ ,
 /*B9C1*/ GENx___x___x___ ,
 /*B9C2*/ GENx___x___x___ ,
 /*B9C3*/ GENx___x___x___ ,
 /*B9C4*/ GENx___x___x___ ,
 /*B9C5*/ GENx___x___x___ ,
 /*B9C6*/ GENx___x___x___ ,
 /*B9C7*/ GENx___x___x___ ,
 /*B9C8*/ GENx___x___x___ ,
 /*B9C9*/ GENx___x___x___ ,
 /*B9CA*/ GENx___x___x___ ,
 /*B9CB*/ GENx___x___x___ ,
 /*B9CC*/ GENx___x___x___ ,
 /*B9CD*/ GENx___x___x___ ,
 /*B9CE*/ GENx___x___x___ ,
 /*B9CF*/ GENx___x___x___ ,
 /*B9D0*/ GENx___x___x___ ,
 /*B9D1*/ GENx___x___x___ ,
 /*B9D2*/ GENx___x___x___ ,
 /*B9D3*/ GENx___x___x___ ,
 /*B9D4*/ GENx___x___x___ ,
 /*B9D5*/ GENx___x___x___ ,
 /*B9D6*/ GENx___x___x___ ,
 /*B9D7*/ GENx___x___x___ ,
 /*B9D8*/ GENx___x___x___ ,
 /*B9D9*/ GENx___x___x___ ,
 /*B9DA*/ GENx___x___x___ ,
 /*B9DB*/ GENx___x___x___ ,
 /*B9DC*/ GENx___x___x___ ,
 /*B9DD*/ GENx___x___x___ ,
 /*B9DE*/ GENx___x___x___ ,
 /*B9DF*/ GENx___x___x___ ,
 /*B9E0*/ GENx___x___x___ ,
 /*B9E1*/ GENx___x___x___ ,
 /*B9E2*/ GENx___x___x___ ,
 /*B9E3*/ GENx___x___x___ ,
 /*B9E4*/ GENx___x___x___ ,
 /*B9E5*/ GENx___x___x___ ,
 /*B9E6*/ GENx___x___x___ ,
 /*B9E7*/ GENx___x___x___ ,
 /*B9E8*/ GENx___x___x___ ,
 /*B9E9*/ GENx___x___x___ ,
 /*B9EA*/ GENx___x___x___ ,
 /*B9EB*/ GENx___x___x___ ,
 /*B9EC*/ GENx___x___x___ ,
 /*B9ED*/ GENx___x___x___ ,
 /*B9EE*/ GENx___x___x___ ,
 /*B9EF*/ GENx___x___x___ ,
 /*B9F0*/ GENx___x___x___ ,
 /*B9F1*/ GENx___x___x___ ,
 /*B9F2*/ GENx___x___x___ ,
 /*B9F3*/ GENx___x___x___ ,
 /*B9F4*/ GENx___x___x___ ,
 /*B9F5*/ GENx___x___x___ ,
 /*B9F6*/ GENx___x___x___ ,
 /*B9F7*/ GENx___x___x___ ,
 /*B9F8*/ GENx___x___x___ ,
 /*B9F9*/ GENx___x___x___ ,
 /*B9FA*/ GENx___x___x___ ,
 /*B9FB*/ GENx___x___x___ ,
 /*B9FC*/ GENx___x___x___ ,
 /*B9FD*/ GENx___x___x___ ,
 /*B9FE*/ GENx___x___x___ ,
 /*B9FF*/ GENx___x___x___  };

// #endif /*defined(FEATURE_ESAME)*/

// #if defined(FEATURE_ESAME)

zz_func opcode_c0xx[16][GEN_MAXARCH] = {
 /*C0x0*/ GENx___x390x900 (load_address_relative_long,RIL,"LARL"),
 /*C0x1*/ GENx___x___x___ ,
 /*C0x2*/ GENx___x___x___ ,
 /*C0x3*/ GENx___x___x___ ,
 /*C0x4*/ GENx___x390x900 (branch_relative_on_condition_long,RIL,"BRCL"),
 /*C0x5*/ GENx___x390x900 (branch_relative_and_save_long,RIL,"BRASL"),
 /*C0x6*/ GENx___x___x___ ,
 /*C0x7*/ GENx___x___x___ ,
 /*C0x8*/ GENx___x___x___ ,
 /*C0x9*/ GENx___x___x___ ,
 /*C0xA*/ GENx___x___x___ ,
 /*C0xB*/ GENx___x___x___ ,
 /*C0xC*/ GENx___x___x___ ,
 /*C0xD*/ GENx___x___x___ ,
 /*C0xE*/ GENx___x___x___ ,
 /*C0xF*/ GENx___x___x___  };

// #endif /*defined(FEATURE_ESAME)*/

// #if defined(FEATURE_ESAME)

zz_func opcode_e3xx[256][GEN_MAXARCH] = {
 /*E300*/ GENx___x___x___ ,
 /*E301*/ GENx___x___x___ ,
 /*E302*/ GENx___x___x___ ,
 /*E303*/ GENx___x___x900 (load_real_address_long,RXE,"LRAG"),
 /*E304*/ GENx___x___x900 (load_long,RXE,"LG"),
 /*E305*/ GENx___x___x___ ,
 /*E306*/ GENx___x___x___ ,
 /*E307*/ GENx___x___x___ ,
 /*E308*/ GENx___x___x900 (add_long,RXE,"AG"),
 /*E309*/ GENx___x___x900 (subtract_long,RXE,"SG"),
 /*E30A*/ GENx___x___x900 (add_logical_long,RXE,"ALG"),
 /*E30B*/ GENx___x___x900 (subtract_logical_long,RXE,"SLG"),
 /*E30C*/ GENx___x___x900 (multiply_single_long,RXE,"MSG"),
 /*E30D*/ GENx___x___x900 (divide_single_long,RXE,"DSG"),
 /*E30E*/ GENx___x___x900 (convert_to_binary_long,RXE,"CVBG"),
 /*E30F*/ GENx___x___x900 (load_reversed_long,RXE,"LRVG"),
 /*E310*/ GENx___x___x___ ,
 /*E311*/ GENx___x___x___ ,
 /*E312*/ GENx___x___x___ ,
 /*E313*/ GENx___x___x___ ,
 /*E314*/ GENx___x___x900 (load_long_fullword,RXE,"LGF"),
 /*E315*/ GENx___x___x900 (load_long_halfword,RXE,"LGH"),
 /*E316*/ GENx___x___x900 (load_logical_long_fullword,RXE,"LLGF"),
 /*E317*/ GENx___x___x900 (load_logical_long_thirtyone,RXE,"LLGT"),
 /*E318*/ GENx___x___x900 (add_long_fullword,RXE,"AGF"),
 /*E319*/ GENx___x___x900 (subtract_long_fullword,RXE,"SGF"),
 /*E31A*/ GENx___x___x900 (add_logical_long_fullword,RXE,"ALGF"),
 /*E31B*/ GENx___x___x900 (subtract_logical_long_fullword,RXE,"SLGF"),
 /*E31C*/ GENx___x___x900 (multiply_single_long_fullword,RXE,"MSGF"),
 /*E31D*/ GENx___x___x900 (divide_single_long_fullword,RXE,"DSGF"),
 /*E31E*/ GENx___x390x900 (load_reversed,RXE,"LRV"),
 /*E31F*/ GENx___x390x900 (load_reversed_half,RXE,"LRVH"),
 /*E320*/ GENx___x___x900 (compare_long,RXE,"CG"),
 /*E321*/ GENx___x___x900 (compare_logical_long,RXE,"CLG"),
 /*E322*/ GENx___x___x___ ,
 /*E323*/ GENx___x___x___ ,
 /*E324*/ GENx___x___x900 (store_long,RXE,"STG"),
 /*E325*/ GENx___x___x___ ,
 /*E326*/ GENx___x___x___ ,
 /*E327*/ GENx___x___x___ ,
 /*E328*/ GENx___x___x___ ,
 /*E329*/ GENx___x___x___ ,
 /*E32A*/ GENx___x___x___ ,
 /*E32B*/ GENx___x___x___ ,
 /*E32C*/ GENx___x___x___ ,
 /*E32D*/ GENx___x___x___ ,
 /*E32E*/ GENx___x___x900 (convert_to_decimal_long,RXE,"CVDG"),
 /*E32F*/ GENx___x___x900 (store_reversed_long,RXE,"STRVG"),
 /*E330*/ GENx___x___x900 (compare_long_fullword,RXE,"CGF"),
 /*E331*/ GENx___x___x900 (compare_logical_long_fullword,RXE,"CLGF"),
 /*E332*/ GENx___x___x___ ,
 /*E333*/ GENx___x___x___ ,
 /*E334*/ GENx___x___x___ ,
 /*E335*/ GENx___x___x___ ,
 /*E336*/ GENx___x___x___ ,
 /*E337*/ GENx___x___x___ ,
 /*E338*/ GENx___x___x___ ,
 /*E339*/ GENx___x___x___ ,
 /*E33A*/ GENx___x___x___ ,
 /*E33B*/ GENx___x___x___ ,
 /*E33C*/ GENx___x___x___ ,
 /*E33D*/ GENx___x___x___ ,
 /*E33E*/ GENx___x390x900 (store_reversed,RXE,"STRV"),
 /*E33F*/ GENx___x390x900 (store_reversed_half,RXE,"STRVH"),
 /*E340*/ GENx___x___x___ ,
 /*E341*/ GENx___x___x___ ,
 /*E342*/ GENx___x___x___ ,
 /*E343*/ GENx___x___x___ ,
 /*E344*/ GENx___x___x___ ,
 /*E345*/ GENx___x___x___ ,
 /*E346*/ GENx___x___x900 (branch_on_count_long,RXE,"BCTG"),
 /*E347*/ GENx___x___x___ ,
 /*E348*/ GENx___x___x___ ,
 /*E349*/ GENx___x___x___ ,
 /*E34A*/ GENx___x___x___ ,
 /*E34B*/ GENx___x___x___ ,
 /*E34C*/ GENx___x___x___ ,
 /*E34D*/ GENx___x___x___ ,
 /*E34E*/ GENx___x___x___ ,
 /*E34F*/ GENx___x___x___ ,
 /*E350*/ GENx___x___x___ ,
 /*E351*/ GENx___x___x___ ,
 /*E352*/ GENx___x___x___ ,
 /*E353*/ GENx___x___x___ ,
 /*E354*/ GENx___x___x___ ,
 /*E355*/ GENx___x___x___ ,
 /*E356*/ GENx___x___x___ ,
 /*E357*/ GENx___x___x___ ,
 /*E358*/ GENx___x___x___ ,
 /*E359*/ GENx___x___x___ ,
 /*E35A*/ GENx___x___x___ ,
 /*E35B*/ GENx___x___x___ ,
 /*E35C*/ GENx___x___x___ ,
 /*E35D*/ GENx___x___x___ ,
 /*E35E*/ GENx___x___x___ ,
 /*E35F*/ GENx___x___x___ ,
 /*E360*/ GENx___x___x___ ,
 /*E361*/ GENx___x___x___ ,
 /*E362*/ GENx___x___x___ ,
 /*E363*/ GENx___x___x___ ,
 /*E364*/ GENx___x___x___ ,
 /*E365*/ GENx___x___x___ ,
 /*E366*/ GENx___x___x___ ,
 /*E367*/ GENx___x___x___ ,
 /*E368*/ GENx___x___x___ ,
 /*E369*/ GENx___x___x___ ,
 /*E36A*/ GENx___x___x___ ,
 /*E36B*/ GENx___x___x___ ,
 /*E36C*/ GENx___x___x___ ,
 /*E36D*/ GENx___x___x___ ,
 /*E36E*/ GENx___x___x___ ,
 /*E36F*/ GENx___x___x___ ,
 /*E370*/ GENx___x___x___ ,
 /*E371*/ GENx___x___x___ ,
 /*E372*/ GENx___x___x___ ,
 /*E373*/ GENx___x___x___ ,
 /*E374*/ GENx___x___x___ ,
 /*E375*/ GENx___x___x___ ,
 /*E376*/ GENx___x___x___ ,
 /*E377*/ GENx___x___x___ ,
 /*E378*/ GENx___x___x___ ,
 /*E379*/ GENx___x___x___ ,
 /*E37A*/ GENx___x___x___ ,
 /*E37B*/ GENx___x___x___ ,
 /*E37C*/ GENx___x___x___ ,
 /*E37D*/ GENx___x___x___ ,
 /*E37E*/ GENx___x___x___ ,
 /*E37F*/ GENx___x___x___ ,
 /*E380*/ GENx___x___x900 (and_long,RXE,"NG"),
 /*E381*/ GENx___x___x900 (or_long,RXE,"OG"),
 /*E382*/ GENx___x___x900 (exclusive_or_long,RXE,"XG"),
 /*E383*/ GENx___x___x___ ,
 /*E384*/ GENx___x___x___ ,
 /*E385*/ GENx___x___x___ ,
 /*E386*/ GENx___x___x900 (multiply_logical_long,RXE,"MLG"),
 /*E387*/ GENx___x___x900 (divide_logical_long,RXE,"DLG"),
 /*E388*/ GENx___x___x900 (add_logical_carry_long,RXE,"ALCG"),
 /*E389*/ GENx___x___x900 (subtract_logical_borrow_long,RXE,"SLBG"),
 /*E38A*/ GENx___x___x___ ,
 /*E38B*/ GENx___x___x___ ,
 /*E38C*/ GENx___x___x___ ,
 /*E38D*/ GENx___x___x___ ,
 /*E38E*/ GENx___x___x900 (store_pair_to_quadword,RXE,"STPQ"),
 /*E38F*/ GENx___x___x900 (load_pair_from_quadword,RXE,"LPQ"),
 /*E390*/ GENx___x___x900 (load_logical_character,RXE,"LLGC"),
 /*E391*/ GENx___x___x900 (load_logical_halfword,RXE,"LLGH"),
 /*E392*/ GENx___x___x___ ,
 /*E393*/ GENx___x___x___ ,
 /*E394*/ GENx___x___x___ ,
 /*E395*/ GENx___x___x___ ,
 /*E396*/ GENx___x390x900 (multiply_logical,RXE,"ML"),
 /*E397*/ GENx___x390x900 (divide_logical,RXE,"DL"),
 /*E398*/ GENx___x390x900 (add_logical_carry,RXE,"ALC"),
 /*E399*/ GENx___x390x900 (subtract_logical_borrow,RXE,"SLB"),
 /*E39A*/ GENx___x___x___ ,
 /*E39B*/ GENx___x___x___ ,
 /*E39C*/ GENx___x___x___ ,
 /*E39D*/ GENx___x___x___ ,
 /*E39E*/ GENx___x___x___ ,
 /*E39F*/ GENx___x___x___ ,
 /*E3A0*/ GENx___x___x___ ,
 /*E3A1*/ GENx___x___x___ ,
 /*E3A2*/ GENx___x___x___ ,
 /*E3A3*/ GENx___x___x___ ,
 /*E3E3*/ GENx___x___x___ ,
 /*E3A5*/ GENx___x___x___ ,
 /*E3A6*/ GENx___x___x___ ,
 /*E3A7*/ GENx___x___x___ ,
 /*E3A8*/ GENx___x___x___ ,
 /*E3A9*/ GENx___x___x___ ,
 /*E3AA*/ GENx___x___x___ ,
 /*E3AB*/ GENx___x___x___ ,
 /*E3AC*/ GENx___x___x___ ,
 /*E3AD*/ GENx___x___x___ ,
 /*E3AE*/ GENx___x___x___ ,
 /*E3AF*/ GENx___x___x___ ,
 /*E3B0*/ GENx___x___x___ ,
 /*E3B1*/ GENx___x___x___ ,
 /*E3B2*/ GENx___x___x___ ,
 /*E3B3*/ GENx___x___x___ ,
 /*E3B4*/ GENx___x___x___ ,
 /*E3B5*/ GENx___x___x___ ,
 /*E3B6*/ GENx___x___x___ ,
 /*E3B7*/ GENx___x___x___ ,
 /*E3B8*/ GENx___x___x___ ,
 /*E3E3*/ GENx___x___x___ ,
 /*E3BA*/ GENx___x___x___ ,
 /*E3BB*/ GENx___x___x___ ,
 /*E3BC*/ GENx___x___x___ ,
 /*E3BD*/ GENx___x___x___ ,
 /*E3BE*/ GENx___x___x___ ,
 /*E3BF*/ GENx___x___x___ ,
 /*E3C0*/ GENx___x___x___ ,
 /*E3C1*/ GENx___x___x___ ,
 /*E3C2*/ GENx___x___x___ ,
 /*E3C3*/ GENx___x___x___ ,
 /*E3C4*/ GENx___x___x___ ,
 /*E3C5*/ GENx___x___x___ ,
 /*E3C6*/ GENx___x___x___ ,
 /*E3C7*/ GENx___x___x___ ,
 /*E3C8*/ GENx___x___x___ ,
 /*E3C9*/ GENx___x___x___ ,
 /*E3CA*/ GENx___x___x___ ,
 /*E3CB*/ GENx___x___x___ ,
 /*E3CC*/ GENx___x___x___ ,
 /*E3CD*/ GENx___x___x___ ,
 /*E3CE*/ GENx___x___x___ ,
 /*E3CF*/ GENx___x___x___ ,
 /*E3D0*/ GENx___x___x___ ,
 /*E3D1*/ GENx___x___x___ ,
 /*E3D2*/ GENx___x___x___ ,
 /*E3D3*/ GENx___x___x___ ,
 /*E3D4*/ GENx___x___x___ ,
 /*E3D5*/ GENx___x___x___ ,
 /*E3D6*/ GENx___x___x___ ,
 /*E3D7*/ GENx___x___x___ ,
 /*E3D8*/ GENx___x___x___ ,
 /*E3D9*/ GENx___x___x___ ,
 /*E3DA*/ GENx___x___x___ ,
 /*E3DB*/ GENx___x___x___ ,
 /*E3DC*/ GENx___x___x___ ,
 /*E3DD*/ GENx___x___x___ ,
 /*E3DE*/ GENx___x___x___ ,
 /*E3DF*/ GENx___x___x___ ,
 /*E3E0*/ GENx___x___x___ ,
 /*E3E1*/ GENx___x___x___ ,
 /*E3E2*/ GENx___x___x___ ,
 /*E3E3*/ GENx___x___x___ ,
 /*E3E4*/ GENx___x___x___ ,
 /*E3E5*/ GENx___x___x___ ,
 /*E3E6*/ GENx___x___x___ ,
 /*E3E7*/ GENx___x___x___ ,
 /*E3E8*/ GENx___x___x___ ,
 /*E3E9*/ GENx___x___x___ ,
 /*E3EA*/ GENx___x___x___ ,
 /*E3EB*/ GENx___x___x___ ,
 /*E3EC*/ GENx___x___x___ ,
 /*E3ED*/ GENx___x___x___ ,
 /*E3EE*/ GENx___x___x___ ,
 /*E3EF*/ GENx___x___x___ ,
 /*E3F0*/ GENx___x___x___ ,
 /*E3F1*/ GENx___x___x___ ,
 /*E3F2*/ GENx___x___x___ ,
 /*E3F3*/ GENx___x___x___ ,
 /*E3F4*/ GENx___x___x___ ,
 /*E3F5*/ GENx___x___x___ ,
 /*E3F6*/ GENx___x___x___ ,
 /*E3F7*/ GENx___x___x___ ,
 /*E3F8*/ GENx___x___x___ ,
 /*E3F9*/ GENx___x___x___ ,
 /*E3FA*/ GENx___x___x___ ,
 /*E3FB*/ GENx___x___x___ ,
 /*E3FC*/ GENx___x___x___ ,
 /*E3FD*/ GENx___x___x___ ,
 /*E3FE*/ GENx___x___x___ ,
 /*E3FF*/ GENx___x___x___  };

// #endif /*defined(FEATURE_ESAME)*/

zz_func opcode_e5xx[256][GEN_MAXARCH] = {
 /*E500*/ GENx370x390x900 (load_address_space_parameters,SSE,"LASP"),
 /*E501*/ GENx370x390x900 (test_protection,SSE,"TPROT"),
 /*E502*/ GENx___x___x900 (store_real_address,SSE,"STRAG"),
 /*E503*/ GENx370x390x___ (svc_assist,SSE,"Assist"),
 /*E504*/ GENx370x390x900 (obtain_local_lock,SSE,"Assist"),
 /*E505*/ GENx370x390x900 (release_local_lock,SSE,"Assist"),
 /*E506*/ GENx370x390x900 (obtain_cms_lock,SSE,"Assist"),
 /*E507*/ GENx370x390x900 (release_cms_lock,SSE,"Assist"),
 /*E508*/ GENx370x___x___ (trace_svc_interruption,SSE,"Assist"),
 /*E509*/ GENx370x___x___ (trace_program_interruption,SSE,"Assist"),
 /*E50A*/ GENx370x___x___ (trace_initial_srb_dispatch,SSE,"Assist"),
 /*E50B*/ GENx370x___x___ (trace_io_interruption,SSE,"Assist"),
 /*E50C*/ GENx370x___x___ (trace_task_dispatch,SSE,"Assist"),
 /*E50D*/ GENx370x___x___ (trace_svc_return,SSE,"Assist"),
 /*E50E*/ GENx___x390x900 (move_with_source_key,SSE,"MVCSK"),
 /*E50F*/ GENx___x390x900 (move_with_destination_key,SSE,"MVCDK"),
 /*E510*/ GENx___x___x___ ,
 /*E511*/ GENx___x___x___ ,
 /*E512*/ GENx___x___x___ ,
 /*E513*/ GENx___x___x___ ,
 /*E514*/ GENx___x___x___ ,
 /*E515*/ GENx___x___x___ ,
 /*E516*/ GENx___x___x___ ,
 /*E517*/ GENx___x___x___ ,
 /*E518*/ GENx___x___x___ ,
 /*E519*/ GENx___x___x___ ,
 /*E51A*/ GENx___x___x___ ,
 /*E51B*/ GENx___x___x___ ,
 /*E51C*/ GENx___x___x___ ,
 /*E51D*/ GENx___x___x___ ,
 /*E51E*/ GENx___x___x___ ,
 /*E51F*/ GENx___x___x___ ,
 /*E520*/ GENx___x___x___ ,
 /*E521*/ GENx___x___x___ ,
 /*E522*/ GENx___x___x___ ,
 /*E523*/ GENx___x___x___ ,
 /*E524*/ GENx___x___x___ ,
 /*E525*/ GENx___x___x___ ,
 /*E526*/ GENx___x___x___ ,
 /*E527*/ GENx___x___x___ ,
 /*E528*/ GENx___x___x___ ,
 /*E529*/ GENx___x___x___ ,
 /*E52A*/ GENx___x___x___ ,
 /*E52B*/ GENx___x___x___ ,
 /*E52C*/ GENx___x___x___ ,
 /*E52D*/ GENx___x___x___ ,
 /*E52E*/ GENx___x___x___ ,
 /*E52F*/ GENx___x___x___ ,
 /*E530*/ GENx___x___x___ ,
 /*E531*/ GENx___x___x___ ,
 /*E532*/ GENx___x___x___ ,
 /*E533*/ GENx___x___x___ ,
 /*E534*/ GENx___x___x___ ,
 /*E535*/ GENx___x___x___ ,
 /*E536*/ GENx___x___x___ ,
 /*E537*/ GENx___x___x___ ,
 /*E538*/ GENx___x___x___ ,
 /*E539*/ GENx___x___x___ ,
 /*E53A*/ GENx___x___x___ ,
 /*E53B*/ GENx___x___x___ ,
 /*E53C*/ GENx___x___x___ ,
 /*E53D*/ GENx___x___x___ ,
 /*E53E*/ GENx___x___x___ ,
 /*E53F*/ GENx___x___x___ ,
 /*E540*/ GENx___x___x___ ,
 /*E541*/ GENx___x___x___ ,
 /*E542*/ GENx___x___x___ ,
 /*E543*/ GENx___x___x___ ,
 /*E544*/ GENx___x___x___ ,
 /*E545*/ GENx___x___x___ ,
 /*E546*/ GENx___x___x___ ,
 /*E547*/ GENx___x___x___ ,
 /*E548*/ GENx___x___x___ ,
 /*E549*/ GENx___x___x___ ,
 /*E54A*/ GENx___x___x___ ,
 /*E54B*/ GENx___x___x___ ,
 /*E54C*/ GENx___x___x___ ,
 /*E54D*/ GENx___x___x___ ,
 /*E54E*/ GENx___x___x___ ,
 /*E54F*/ GENx___x___x___ ,
 /*E550*/ GENx___x___x___ ,
 /*E551*/ GENx___x___x___ ,
 /*E552*/ GENx___x___x___ ,
 /*E553*/ GENx___x___x___ ,
 /*E554*/ GENx___x___x___ ,
 /*E555*/ GENx___x___x___ ,
 /*E556*/ GENx___x___x___ ,
 /*E557*/ GENx___x___x___ ,
 /*E558*/ GENx___x___x___ ,
 /*E559*/ GENx___x___x___ ,
 /*E55A*/ GENx___x___x___ ,
 /*E55B*/ GENx___x___x___ ,
 /*E55C*/ GENx___x___x___ ,
 /*E55D*/ GENx___x___x___ ,
 /*E55E*/ GENx___x___x___ ,
 /*E55F*/ GENx___x___x___ ,
 /*E560*/ GENx___x___x___ ,
 /*E561*/ GENx___x___x___ ,
 /*E562*/ GENx___x___x___ ,
 /*E563*/ GENx___x___x___ ,
 /*E564*/ GENx___x___x___ ,
 /*E565*/ GENx___x___x___ ,
 /*E566*/ GENx___x___x___ ,
 /*E567*/ GENx___x___x___ ,
 /*E568*/ GENx___x___x___ ,
 /*E569*/ GENx___x___x___ ,
 /*E56A*/ GENx___x___x___ ,
 /*E56B*/ GENx___x___x___ ,
 /*E56C*/ GENx___x___x___ ,
 /*E56D*/ GENx___x___x___ ,
 /*E56E*/ GENx___x___x___ ,
 /*E56F*/ GENx___x___x___ ,
 /*E570*/ GENx___x___x___ ,
 /*E571*/ GENx___x___x___ ,
 /*E572*/ GENx___x___x___ ,
 /*E573*/ GENx___x___x___ ,
 /*E574*/ GENx___x___x___ ,
 /*E575*/ GENx___x___x___ ,
 /*E576*/ GENx___x___x___ ,
 /*E577*/ GENx___x___x___ ,
 /*E578*/ GENx___x___x___ ,
 /*E579*/ GENx___x___x___ ,
 /*E57A*/ GENx___x___x___ ,
 /*E57B*/ GENx___x___x___ ,
 /*E57C*/ GENx___x___x___ ,
 /*E57D*/ GENx___x___x___ ,
 /*E57E*/ GENx___x___x___ ,
 /*E57F*/ GENx___x___x___ ,
 /*E580*/ GENx___x___x___ ,
 /*E581*/ GENx___x___x___ ,
 /*E582*/ GENx___x___x___ ,
 /*E583*/ GENx___x___x___ ,
 /*E584*/ GENx___x___x___ ,
 /*E585*/ GENx___x___x___ ,
 /*E586*/ GENx___x___x___ ,
 /*E587*/ GENx___x___x___ ,
 /*E588*/ GENx___x___x___ ,
 /*E589*/ GENx___x___x___ ,
 /*E58A*/ GENx___x___x___ ,
 /*E58B*/ GENx___x___x___ ,
 /*E58C*/ GENx___x___x___ ,
 /*E58D*/ GENx___x___x___ ,
 /*E58E*/ GENx___x___x___ ,
 /*E58F*/ GENx___x___x___ ,
 /*E590*/ GENx___x___x___ ,
 /*E591*/ GENx___x___x___ ,
 /*E592*/ GENx___x___x___ ,
 /*E593*/ GENx___x___x___ ,
 /*E594*/ GENx___x___x___ ,
 /*E595*/ GENx___x___x___ ,
 /*E596*/ GENx___x___x___ ,
 /*E597*/ GENx___x___x___ ,
 /*E598*/ GENx___x___x___ ,
 /*E599*/ GENx___x___x___ ,
 /*E59A*/ GENx___x___x___ ,
 /*E59B*/ GENx___x___x___ ,
 /*E59C*/ GENx___x___x___ ,
 /*E59D*/ GENx___x___x___ ,
 /*E59E*/ GENx___x___x___ ,
 /*E59F*/ GENx___x___x___ ,
 /*E5A0*/ GENx___x___x___ ,
 /*E5A1*/ GENx___x___x___ ,
 /*E5A2*/ GENx___x___x___ ,
 /*E5A3*/ GENx___x___x___ ,
 /*E5A4*/ GENx___x___x___ ,
 /*E5A5*/ GENx___x___x___ ,
 /*E5A6*/ GENx___x___x___ ,
 /*E5A7*/ GENx___x___x___ ,
 /*E5A8*/ GENx___x___x___ ,
 /*E5A9*/ GENx___x___x___ ,
 /*E5AA*/ GENx___x___x___ ,
 /*E5AB*/ GENx___x___x___ ,
 /*E5AC*/ GENx___x___x___ ,
 /*E5AD*/ GENx___x___x___ ,
 /*E5AE*/ GENx___x___x___ ,
 /*E5AF*/ GENx___x___x___ ,
 /*E5B0*/ GENx___x___x___ ,
 /*E5B1*/ GENx___x___x___ ,
 /*E5B2*/ GENx___x___x___ ,
 /*E5B3*/ GENx___x___x___ ,
 /*E5B4*/ GENx___x___x___ ,
 /*E5B5*/ GENx___x___x___ ,
 /*E5B6*/ GENx___x___x___ ,
 /*E5B7*/ GENx___x___x___ ,
 /*E5B8*/ GENx___x___x___ ,
 /*E5B9*/ GENx___x___x___ ,
 /*E5BA*/ GENx___x___x___ ,
 /*E5BB*/ GENx___x___x___ ,
 /*E5BC*/ GENx___x___x___ ,
 /*E5BD*/ GENx___x___x___ ,
 /*E5BE*/ GENx___x___x___ ,
 /*E5BF*/ GENx___x___x___ ,
 /*E5C0*/ GENx___x___x___ ,
 /*E5C1*/ GENx___x___x___ ,
 /*E5C2*/ GENx___x___x___ ,
 /*E5C3*/ GENx___x___x___ ,
 /*E5C4*/ GENx___x___x___ ,
 /*E5C5*/ GENx___x___x___ ,
 /*E5C6*/ GENx___x___x___ ,
 /*E5C7*/ GENx___x___x___ ,
 /*E5C8*/ GENx___x___x___ ,
 /*E5C9*/ GENx___x___x___ ,
 /*E5CA*/ GENx___x___x___ ,
 /*E5CB*/ GENx___x___x___ ,
 /*E5CC*/ GENx___x___x___ ,
 /*E5CD*/ GENx___x___x___ ,
 /*E5CE*/ GENx___x___x___ ,
 /*E5CF*/ GENx___x___x___ ,
 /*E5D0*/ GENx___x___x___ ,
 /*E5D1*/ GENx___x___x___ ,
 /*E5D2*/ GENx___x___x___ ,
 /*E5D3*/ GENx___x___x___ ,
 /*E5D4*/ GENx___x___x___ ,
 /*E5D5*/ GENx___x___x___ ,
 /*E5D6*/ GENx___x___x___ ,
 /*E5D7*/ GENx___x___x___ ,
 /*E5D8*/ GENx___x___x___ ,
 /*E5D9*/ GENx___x___x___ ,
 /*E5DA*/ GENx___x___x___ ,
 /*E5DB*/ GENx___x___x___ ,
 /*E5DC*/ GENx___x___x___ ,
 /*E5DD*/ GENx___x___x___ ,
 /*E5DE*/ GENx___x___x___ ,
 /*E5DF*/ GENx___x___x___ ,
 /*E5E0*/ GENx___x___x___ ,
 /*E5E1*/ GENx___x___x___ ,
 /*E5E2*/ GENx___x___x___ ,
 /*E5E3*/ GENx___x___x___ ,
 /*E5E4*/ GENx___x___x___ ,
 /*E5E5*/ GENx___x___x___ ,
 /*E5E6*/ GENx___x___x___ ,
 /*E5E7*/ GENx___x___x___ ,
 /*E5E8*/ GENx___x___x___ ,
 /*E5E9*/ GENx___x___x___ ,
 /*E5EA*/ GENx___x___x___ ,
 /*E5EB*/ GENx___x___x___ ,
 /*E5EC*/ GENx___x___x___ ,
 /*E5ED*/ GENx___x___x___ ,
 /*E5EE*/ GENx___x___x___ ,
 /*E5EF*/ GENx___x___x___ ,
 /*E5F0*/ GENx___x___x___ ,
 /*E5F1*/ GENx___x___x___ ,
 /*E5F2*/ GENx___x___x___ ,
 /*E5F3*/ GENx___x___x___ ,
 /*E5F4*/ GENx___x___x___ ,
 /*E5F5*/ GENx___x___x___ ,
 /*E5F6*/ GENx___x___x___ ,
 /*E5F7*/ GENx___x___x___ ,
 /*E5F8*/ GENx___x___x___ ,
 /*E5F9*/ GENx___x___x___ ,
 /*E5FA*/ GENx___x___x___ ,
 /*E5FB*/ GENx___x___x___ ,
 /*E5FC*/ GENx___x___x___ ,
 /*E5FD*/ GENx___x___x___ ,
 /*E5FE*/ GENx___x___x___ ,
 /*E5FF*/ GENx___x___x___  };


// #if defined(FEATURE_ESAME)

/* opcodes EBxxxxxx00 */
zz_func opcode_ebxx[256][GEN_MAXARCH] = {
 /*EB00*/ GENx___x___x___ ,
 /*EB01*/ GENx___x___x___ ,
 /*EB02*/ GENx___x___x___ ,
 /*EB03*/ GENx___x___x___ ,
 /*EB04*/ GENx___x___x900 (load_multiple_long,RSE,"LMG"),
 /*EB05*/ GENx___x___x___ ,
 /*EB06*/ GENx___x___x___ ,
 /*EB07*/ GENx___x___x___ ,
 /*EB08*/ GENx___x___x___ ,
 /*EB09*/ GENx___x___x___ ,
 /*EB0A*/ GENx___x___x900 (shift_right_single_long,RSE,"SRAG"),
 /*EB0B*/ GENx___x___x900 (shift_left_single_long,RSE,"SLAG"),
 /*EB0C*/ GENx___x___x900 (shift_right_single_logical_long,RSE,"SRLG"),
 /*EB0D*/ GENx___x___x900 (shift_left_single_logical_long,RSE,"SLLG"),
 /*EB0E*/ GENx___x___x___ ,
 /*EB0F*/ GENx___x___x900 (trace_long,RSE,"TRACG"),
 /*EB10*/ GENx___x___x___ ,
 /*EB11*/ GENx___x___x___ ,
 /*EB12*/ GENx___x___x___ ,
 /*EB13*/ GENx___x___x___ ,
 /*EB14*/ GENx___x___x___ ,
 /*EB15*/ GENx___x___x___ ,
 /*EB16*/ GENx___x___x___ ,
 /*EB17*/ GENx___x___x___ ,
 /*EB18*/ GENx___x___x___ ,
 /*EB19*/ GENx___x___x___ ,
 /*EB1A*/ GENx___x___x___ ,
 /*EB1B*/ GENx___x___x___ ,
 /*EB1C*/ GENx___x___x900 (rotate_left_single_logical_long,RSE,"RLLG"),
 /*EB1D*/ GENx___x390x900 (rotate_left_single_logical,RSE,"RLL"),
 /*EB1E*/ GENx___x___x___ ,
 /*EB1F*/ GENx___x___x___ ,
 /*EB20*/ GENx___x___x900 (compare_logical_characters_under_mask_high,RSE,"CLMH"),
 /*EB21*/ GENx___x___x___ ,
 /*EB22*/ GENx___x___x___ ,
 /*EB23*/ GENx___x___x___ ,
 /*EB24*/ GENx___x___x900 (store_multiple_long,RSE,"STMG"),
 /*EB25*/ GENx___x___x900 (store_control_long,RSE,"STCTG"),
 /*EB26*/ GENx___x___x900 (store_multiple_high,RSE,"STMH"),
 /*EB27*/ GENx___x___x___ ,
 /*EB28*/ GENx___x___x___ ,
 /*EB29*/ GENx___x___x___ ,
 /*EB2A*/ GENx___x___x___ ,
 /*EB2B*/ GENx___x___x___ ,
 /*EB2C*/ GENx___x___x900 (store_characters_under_mask_high,RSE,"STCMH"),
 /*EB2D*/ GENx___x___x___ ,
 /*EB2E*/ GENx___x___x___ ,
 /*EB2F*/ GENx___x___x900 (load_control_long,RSE,"LCTLG"),
 /*EB30*/ GENx___x___x900 (compare_and_swap_long,RSE,"CSG"),
 /*EB31*/ GENx___x___x___ ,
 /*EB32*/ GENx___x___x___ ,
 /*EB33*/ GENx___x___x___ ,
 /*EB34*/ GENx___x___x___ ,
 /*EB35*/ GENx___x___x___ ,
 /*EB36*/ GENx___x___x___ ,
 /*EB37*/ GENx___x___x___ ,
 /*EB38*/ GENx___x___x___ ,
 /*EB39*/ GENx___x___x___ ,
 /*EB3A*/ GENx___x___x___ ,
 /*EB3B*/ GENx___x___x___ ,
 /*EB3C*/ GENx___x___x___ ,
 /*EB3D*/ GENx___x___x___ ,
 /*EB3E*/ GENx___x___x900 (compare_double_and_swap_long,RSE,"CDSG"),
 /*EB3F*/ GENx___x___x___ ,
 /*EB40*/ GENx___x___x___ ,
 /*EB41*/ GENx___x___x___ ,
 /*EB42*/ GENx___x___x___ ,
 /*EB43*/ GENx___x___x___ ,
 /*EB44*/ GENx___x___x900 (branch_on_index_high_long,RSE,"BXHG"),
 /*EB45*/ GENx___x___x900 (branch_on_index_low_or_equal_long,RSE,"BXLEG"),
 /*EB46*/ GENx___x___x___ ,
 /*EB47*/ GENx___x___x___ ,
 /*EB48*/ GENx___x___x___ ,
 /*EB49*/ GENx___x___x___ ,
 /*EB4A*/ GENx___x___x___ ,
 /*EB4B*/ GENx___x___x___ ,
 /*EB4C*/ GENx___x___x___ ,
 /*EB4D*/ GENx___x___x___ ,
 /*EB4E*/ GENx___x___x___ ,
 /*EB4F*/ GENx___x___x___ ,
 /*EB50*/ GENx___x___x___ ,
 /*EB51*/ GENx___x___x___ ,
 /*EB52*/ GENx___x___x___ ,
 /*EB53*/ GENx___x___x___ ,
 /*EB54*/ GENx___x___x___ ,
 /*EB55*/ GENx___x___x___ ,
 /*EB56*/ GENx___x___x___ ,
 /*EB57*/ GENx___x___x___ ,
 /*EB58*/ GENx___x___x___ ,
 /*EB59*/ GENx___x___x___ ,
 /*EB5A*/ GENx___x___x___ ,
 /*EB5B*/ GENx___x___x___ ,
 /*EB5C*/ GENx___x___x___ ,
 /*EB5D*/ GENx___x___x___ ,
 /*EB5E*/ GENx___x___x___ ,
 /*EB5F*/ GENx___x___x___ ,
 /*EB60*/ GENx___x___x___ ,
 /*EB61*/ GENx___x___x___ ,
 /*EB62*/ GENx___x___x___ ,
 /*EB63*/ GENx___x___x___ ,
 /*EB64*/ GENx___x___x___ ,
 /*EB65*/ GENx___x___x___ ,
 /*EB66*/ GENx___x___x___ ,
 /*EB67*/ GENx___x___x___ ,
 /*EB68*/ GENx___x___x___ ,
 /*EB69*/ GENx___x___x___ ,
 /*EB6A*/ GENx___x___x___ ,
 /*EB6B*/ GENx___x___x___ ,
 /*EB6C*/ GENx___x___x___ ,
 /*EB6D*/ GENx___x___x___ ,
 /*EB6E*/ GENx___x___x___ ,
 /*EB6F*/ GENx___x___x___ ,
 /*EB70*/ GENx___x___x___ ,
 /*EB71*/ GENx___x___x___ ,
 /*EB72*/ GENx___x___x___ ,
 /*EB73*/ GENx___x___x___ ,
 /*EB74*/ GENx___x___x___ ,
 /*EB75*/ GENx___x___x___ ,
 /*EB76*/ GENx___x___x___ ,
 /*EB77*/ GENx___x___x___ ,
 /*EB78*/ GENx___x___x___ ,
 /*EB79*/ GENx___x___x___ ,
 /*EB7A*/ GENx___x___x___ ,
 /*EB7B*/ GENx___x___x___ ,
 /*EB7C*/ GENx___x___x___ ,
 /*EB7D*/ GENx___x___x___ ,
 /*EB7E*/ GENx___x___x___ ,
 /*EB7F*/ GENx___x___x___ ,
 /*EB80*/ GENx___x___x900 (insert_characters_under_mask_high,RSE,"ICMH"),
 /*EB81*/ GENx___x___x___ ,
 /*EB82*/ GENx___x___x___ ,
 /*EB83*/ GENx___x___x___ ,
 /*EB84*/ GENx___x___x___ ,
 /*EB85*/ GENx___x___x___ ,
 /*EB86*/ GENx___x___x___ ,
 /*EB87*/ GENx___x___x___ ,
 /*EB88*/ GENx___x___x___ ,
 /*EB89*/ GENx___x___x___ ,
 /*EB8A*/ GENx___x___x___ ,
 /*EB8B*/ GENx___x___x___ ,
 /*EB8C*/ GENx___x___x___ ,
 /*EB8D*/ GENx___x___x___ ,
 /*EB8E*/ GENx___x___x900 (dummy_instruction,RSE,"MVCLU"),
 /*EB8F*/ GENx___x___x900 (dummy_instruction,RSE,"CLCLU"),
 /*EB90*/ GENx___x___x___ ,
 /*EB91*/ GENx___x___x___ ,
 /*EB92*/ GENx___x___x___ ,
 /*EB93*/ GENx___x___x___ ,
 /*EB94*/ GENx___x___x___ ,
 /*EB95*/ GENx___x___x___ ,
 /*EB96*/ GENx___x___x900 (load_multiple_high,RSE,"LMH"),
 /*EB97*/ GENx___x___x___ ,
 /*EB98*/ GENx___x___x___ ,
 /*EB99*/ GENx___x___x___ ,
 /*EB9A*/ GENx___x___x___ ,
 /*EB9B*/ GENx___x___x___ ,
 /*EB9C*/ GENx___x___x___ ,
 /*EB9D*/ GENx___x___x___ ,
 /*EB9E*/ GENx___x___x___ ,
 /*EB9F*/ GENx___x___x___ ,
 /*EBA0*/ GENx___x___x___ ,
 /*EBA1*/ GENx___x___x___ ,
 /*EBA2*/ GENx___x___x___ ,
 /*EBA3*/ GENx___x___x___ ,
 /*EBEB*/ GENx___x___x___ ,
 /*EBA5*/ GENx___x___x___ ,
 /*EBA6*/ GENx___x___x___ ,
 /*EBA7*/ GENx___x___x___ ,
 /*EBA8*/ GENx___x___x___ ,
 /*EBA9*/ GENx___x___x___ ,
 /*EBAA*/ GENx___x___x___ ,
 /*EBAB*/ GENx___x___x___ ,
 /*EBAC*/ GENx___x___x___ ,
 /*EBAD*/ GENx___x___x___ ,
 /*EBAE*/ GENx___x___x___ ,
 /*EBAF*/ GENx___x___x___ ,
 /*EBB0*/ GENx___x___x___ ,
 /*EBB1*/ GENx___x___x___ ,
 /*EBB2*/ GENx___x___x___ ,
 /*EBB3*/ GENx___x___x___ ,
 /*EBB4*/ GENx___x___x___ ,
 /*EBB5*/ GENx___x___x___ ,
 /*EBB6*/ GENx___x___x___ ,
 /*EBB7*/ GENx___x___x___ ,
 /*EBB8*/ GENx___x___x___ ,
 /*EBEB*/ GENx___x___x___ ,
 /*EBBA*/ GENx___x___x___ ,
 /*EBBB*/ GENx___x___x___ ,
 /*EBBC*/ GENx___x___x___ ,
 /*EBBD*/ GENx___x___x___ ,
 /*EBBE*/ GENx___x___x___ ,
 /*EBBF*/ GENx___x___x___ ,
 /*EBC0*/ GENx___x___x900 (test_decimal,RSL,"TP"),
 /*EBC1*/ GENx___x___x___ ,
 /*EBC2*/ GENx___x___x___ ,
 /*EBC3*/ GENx___x___x___ ,
 /*EBC4*/ GENx___x___x___ ,
 /*EBC5*/ GENx___x___x___ ,
 /*EBC6*/ GENx___x___x___ ,
 /*EBC7*/ GENx___x___x___ ,
 /*EBC8*/ GENx___x___x___ ,
 /*EBC9*/ GENx___x___x___ ,
 /*EBCA*/ GENx___x___x___ ,
 /*EBCB*/ GENx___x___x___ ,
 /*EBCC*/ GENx___x___x___ ,
 /*EBCD*/ GENx___x___x___ ,
 /*EBCE*/ GENx___x___x___ ,
 /*EBCF*/ GENx___x___x___ ,
 /*EBD0*/ GENx___x___x___ ,
 /*EBD1*/ GENx___x___x___ ,
 /*EBD2*/ GENx___x___x___ ,
 /*EBD3*/ GENx___x___x___ ,
 /*EBD4*/ GENx___x___x___ ,
 /*EBD5*/ GENx___x___x___ ,
 /*EBD6*/ GENx___x___x___ ,
 /*EBD7*/ GENx___x___x___ ,
 /*EBD8*/ GENx___x___x___ ,
 /*EBD9*/ GENx___x___x___ ,
 /*EBDA*/ GENx___x___x___ ,
 /*EBDB*/ GENx___x___x___ ,
 /*EBDC*/ GENx___x___x___ ,
 /*EBDD*/ GENx___x___x___ ,
 /*EBDE*/ GENx___x___x___ ,
 /*EBDF*/ GENx___x___x___ ,
 /*EBE0*/ GENx___x___x___ ,
 /*EBE1*/ GENx___x___x___ ,
 /*EBE2*/ GENx___x___x___ ,
 /*EBEB*/ GENx___x___x___ ,
 /*EBE4*/ GENx___x___x___ ,
 /*EBE5*/ GENx___x___x___ ,
 /*EBE6*/ GENx___x___x___ ,
 /*EBE7*/ GENx___x___x___ ,
 /*EBE8*/ GENx___x___x___ ,
 /*EBE9*/ GENx___x___x___ ,
 /*EBEA*/ GENx___x___x___ ,
 /*EBEB*/ GENx___x___x___ ,
 /*EBEC*/ GENx___x___x___ ,
 /*EBED*/ GENx___x___x___ ,
 /*EBEE*/ GENx___x___x___ ,
 /*EBEF*/ GENx___x___x___ ,
 /*EBF0*/ GENx___x___x___ ,
 /*EBF1*/ GENx___x___x___ ,
 /*EBF2*/ GENx___x___x___ ,
 /*EBF3*/ GENx___x___x___ ,
 /*EBF4*/ GENx___x___x___ ,
 /*EBF5*/ GENx___x___x___ ,
 /*EBF6*/ GENx___x___x___ ,
 /*EBF7*/ GENx___x___x___ ,
 /*EBF8*/ GENx___x___x___ ,
 /*EBF9*/ GENx___x___x___ ,
 /*EBFA*/ GENx___x___x___ ,
 /*EBFB*/ GENx___x___x___ ,
 /*EBFC*/ GENx___x___x___ ,
 /*EBFD*/ GENx___x___x___ ,
 /*EBFE*/ GENx___x___x___ ,
 /*EBFF*/ GENx___x___x___  };

// #endif /*defined(FEATURE_ESAME)*/

// #if defined(FEATURE_ESAME)

zz_func opcode_ecxx[256][GEN_MAXARCH] = {
 /*EC00*/ GENx___x___x___ ,
 /*EC01*/ GENx___x___x___ ,
 /*EC02*/ GENx___x___x___ ,
 /*EC03*/ GENx___x___x___ ,
 /*EC04*/ GENx___x___x___ ,
 /*EC05*/ GENx___x___x___ ,
 /*EC06*/ GENx___x___x___ ,
 /*EC07*/ GENx___x___x___ ,
 /*EC08*/ GENx___x___x___ ,
 /*EC09*/ GENx___x___x___ ,
 /*EC0A*/ GENx___x___x___ ,
 /*EC0B*/ GENx___x___x___ ,
 /*EC0C*/ GENx___x___x___ ,
 /*EC0D*/ GENx___x___x___ ,
 /*EC0E*/ GENx___x___x___ ,
 /*EC0F*/ GENx___x___x___ ,
 /*EC10*/ GENx___x___x___ ,
 /*EC11*/ GENx___x___x___ ,
 /*EC12*/ GENx___x___x___ ,
 /*EC13*/ GENx___x___x___ ,
 /*EC14*/ GENx___x___x___ ,
 /*EC15*/ GENx___x___x___ ,
 /*EC16*/ GENx___x___x___ ,
 /*EC17*/ GENx___x___x___ ,
 /*EC18*/ GENx___x___x___ ,
 /*EC19*/ GENx___x___x___ ,
 /*EC1A*/ GENx___x___x___ ,
 /*EC1B*/ GENx___x___x___ ,
 /*EC1C*/ GENx___x___x___ ,
 /*EC1D*/ GENx___x___x___ ,
 /*EC1E*/ GENx___x___x___ ,
 /*EC1F*/ GENx___x___x___ ,
 /*EC20*/ GENx___x___x___ ,
 /*EC21*/ GENx___x___x___ ,
 /*EC22*/ GENx___x___x___ ,
 /*EC23*/ GENx___x___x___ ,
 /*EC24*/ GENx___x___x___ ,
 /*EC25*/ GENx___x___x___ ,
 /*EC26*/ GENx___x___x___ ,
 /*EC27*/ GENx___x___x___ ,
 /*EC28*/ GENx___x___x___ ,
 /*EC29*/ GENx___x___x___ ,
 /*EC2A*/ GENx___x___x___ ,
 /*EC2B*/ GENx___x___x___ ,
 /*EC2C*/ GENx___x___x___ ,
 /*EC2D*/ GENx___x___x___ ,
 /*EC2E*/ GENx___x___x___ ,
 /*EC2F*/ GENx___x___x___ ,
 /*EC30*/ GENx___x___x___ ,
 /*EC31*/ GENx___x___x___ ,
 /*EC32*/ GENx___x___x___ ,
 /*EC33*/ GENx___x___x___ ,
 /*EC34*/ GENx___x___x___ ,
 /*EC35*/ GENx___x___x___ ,
 /*EC36*/ GENx___x___x___ ,
 /*EC37*/ GENx___x___x___ ,
 /*EC38*/ GENx___x___x___ ,
 /*EC39*/ GENx___x___x___ ,
 /*EC3A*/ GENx___x___x___ ,
 /*EC3B*/ GENx___x___x___ ,
 /*EC3C*/ GENx___x___x___ ,
 /*EC3D*/ GENx___x___x___ ,
 /*EC3E*/ GENx___x___x___ ,
 /*EC3F*/ GENx___x___x___ ,
 /*EC40*/ GENx___x___x___ ,
 /*EC41*/ GENx___x___x___ ,
 /*EC42*/ GENx___x___x___ ,
 /*EC43*/ GENx___x___x___ ,
 /*EC44*/ GENx___x___x900 (branch_relative_on_index_high_long,RIE,"BRXHG"),
 /*EC45*/ GENx___x___x900 (branch_relative_on_index_low_or_equal_long,RIE,"BRXLG"),
 /*EC46*/ GENx___x___x___ ,
 /*EC47*/ GENx___x___x___ ,
 /*EC48*/ GENx___x___x___ ,
 /*EC49*/ GENx___x___x___ ,
 /*EC4A*/ GENx___x___x___ ,
 /*EC4B*/ GENx___x___x___ ,
 /*EC4C*/ GENx___x___x___ ,
 /*EC4D*/ GENx___x___x___ ,
 /*EC4E*/ GENx___x___x___ ,
 /*EC4F*/ GENx___x___x___ ,
 /*EC50*/ GENx___x___x___ ,
 /*EC51*/ GENx___x___x___ ,
 /*EC52*/ GENx___x___x___ ,
 /*EC53*/ GENx___x___x___ ,
 /*EC54*/ GENx___x___x___ ,
 /*EC55*/ GENx___x___x___ ,
 /*EC56*/ GENx___x___x___ ,
 /*EC57*/ GENx___x___x___ ,
 /*EC58*/ GENx___x___x___ ,
 /*EC59*/ GENx___x___x___ ,
 /*EC5A*/ GENx___x___x___ ,
 /*EC5B*/ GENx___x___x___ ,
 /*EC5C*/ GENx___x___x___ ,
 /*EC5D*/ GENx___x___x___ ,
 /*EC5E*/ GENx___x___x___ ,
 /*EC5F*/ GENx___x___x___ ,
 /*EC60*/ GENx___x___x___ ,
 /*EC61*/ GENx___x___x___ ,
 /*EC62*/ GENx___x___x___ ,
 /*EC63*/ GENx___x___x___ ,
 /*EC64*/ GENx___x___x___ ,
 /*EC65*/ GENx___x___x___ ,
 /*EC66*/ GENx___x___x___ ,
 /*EC67*/ GENx___x___x___ ,
 /*EC68*/ GENx___x___x___ ,
 /*EC69*/ GENx___x___x___ ,
 /*EC6A*/ GENx___x___x___ ,
 /*EC6B*/ GENx___x___x___ ,
 /*EC6C*/ GENx___x___x___ ,
 /*EC6D*/ GENx___x___x___ ,
 /*EC6E*/ GENx___x___x___ ,
 /*EC6F*/ GENx___x___x___ ,
 /*EC70*/ GENx___x___x___ ,
 /*EC71*/ GENx___x___x___ ,
 /*EC72*/ GENx___x___x___ ,
 /*EC73*/ GENx___x___x___ ,
 /*EC74*/ GENx___x___x___ ,
 /*EC75*/ GENx___x___x___ ,
 /*EC76*/ GENx___x___x___ ,
 /*EC77*/ GENx___x___x___ ,
 /*EC78*/ GENx___x___x___ ,
 /*EC79*/ GENx___x___x___ ,
 /*EC7A*/ GENx___x___x___ ,
 /*EC7B*/ GENx___x___x___ ,
 /*EC7C*/ GENx___x___x___ ,
 /*EC7D*/ GENx___x___x___ ,
 /*EC7E*/ GENx___x___x___ ,
 /*EC7F*/ GENx___x___x___ ,
 /*EC80*/ GENx___x___x___ ,
 /*EC81*/ GENx___x___x___ ,
 /*EC82*/ GENx___x___x___ ,
 /*EC83*/ GENx___x___x___ ,
 /*EC84*/ GENx___x___x___ ,
 /*EC85*/ GENx___x___x___ ,
 /*EC86*/ GENx___x___x___ ,
 /*EC87*/ GENx___x___x___ ,
 /*EC88*/ GENx___x___x___ ,
 /*EC89*/ GENx___x___x___ ,
 /*EC8A*/ GENx___x___x___ ,
 /*EC8B*/ GENx___x___x___ ,
 /*EC8C*/ GENx___x___x___ ,
 /*EC8D*/ GENx___x___x___ ,
 /*EC8E*/ GENx___x___x___ ,
 /*EC8F*/ GENx___x___x___ ,
 /*EC90*/ GENx___x___x___ ,
 /*EC91*/ GENx___x___x___ ,
 /*EC92*/ GENx___x___x___ ,
 /*EC93*/ GENx___x___x___ ,
 /*EC94*/ GENx___x___x___ ,
 /*EC95*/ GENx___x___x___ ,
 /*EC96*/ GENx___x___x___ ,
 /*EC97*/ GENx___x___x___ ,
 /*EC98*/ GENx___x___x___ ,
 /*EC99*/ GENx___x___x___ ,
 /*EC9A*/ GENx___x___x___ ,
 /*EC9B*/ GENx___x___x___ ,
 /*EC9C*/ GENx___x___x___ ,
 /*EC9D*/ GENx___x___x___ ,
 /*EC9E*/ GENx___x___x___ ,
 /*EC9F*/ GENx___x___x___ ,
 /*ECA0*/ GENx___x___x___ ,
 /*ECA1*/ GENx___x___x___ ,
 /*ECA2*/ GENx___x___x___ ,
 /*ECA3*/ GENx___x___x___ ,
 /*ECA4*/ GENx___x___x___ ,
 /*ECA5*/ GENx___x___x___ ,
 /*ECA6*/ GENx___x___x___ ,
 /*ECA7*/ GENx___x___x___ ,
 /*ECA8*/ GENx___x___x___ ,
 /*ECA9*/ GENx___x___x___ ,
 /*ECAA*/ GENx___x___x___ ,
 /*ECAB*/ GENx___x___x___ ,
 /*ECAC*/ GENx___x___x___ ,
 /*ECAD*/ GENx___x___x___ ,
 /*ECAE*/ GENx___x___x___ ,
 /*ECAF*/ GENx___x___x___ ,
 /*ECB0*/ GENx___x___x___ ,
 /*ECB1*/ GENx___x___x___ ,
 /*ECB2*/ GENx___x___x___ ,
 /*ECB3*/ GENx___x___x___ ,
 /*ECB4*/ GENx___x___x___ ,
 /*ECB5*/ GENx___x___x___ ,
 /*ECB6*/ GENx___x___x___ ,
 /*ECB7*/ GENx___x___x___ ,
 /*ECB8*/ GENx___x___x___ ,
 /*ECB9*/ GENx___x___x___ ,
 /*ECBA*/ GENx___x___x___ ,
 /*ECBB*/ GENx___x___x___ ,
 /*ECBC*/ GENx___x___x___ ,
 /*ECBD*/ GENx___x___x___ ,
 /*ECBE*/ GENx___x___x___ ,
 /*ECBF*/ GENx___x___x___ ,
 /*ECC0*/ GENx___x___x___ ,
 /*ECC1*/ GENx___x___x___ ,
 /*ECC2*/ GENx___x___x___ ,
 /*ECC3*/ GENx___x___x___ ,
 /*ECC4*/ GENx___x___x___ ,
 /*ECC5*/ GENx___x___x___ ,
 /*ECC6*/ GENx___x___x___ ,
 /*ECC7*/ GENx___x___x___ ,
 /*ECC8*/ GENx___x___x___ ,
 /*ECC9*/ GENx___x___x___ ,
 /*ECCA*/ GENx___x___x___ ,
 /*ECCB*/ GENx___x___x___ ,
 /*ECCC*/ GENx___x___x___ ,
 /*ECCD*/ GENx___x___x___ ,
 /*ECCE*/ GENx___x___x___ ,
 /*ECCF*/ GENx___x___x___ ,
 /*ECD0*/ GENx___x___x___ ,
 /*ECD1*/ GENx___x___x___ ,
 /*ECD2*/ GENx___x___x___ ,
 /*ECD3*/ GENx___x___x___ ,
 /*ECD4*/ GENx___x___x___ ,
 /*ECD5*/ GENx___x___x___ ,
 /*ECD6*/ GENx___x___x___ ,
 /*ECD7*/ GENx___x___x___ ,
 /*ECD8*/ GENx___x___x___ ,
 /*ECD9*/ GENx___x___x___ ,
 /*ECDA*/ GENx___x___x___ ,
 /*ECDB*/ GENx___x___x___ ,
 /*ECDC*/ GENx___x___x___ ,
 /*ECDD*/ GENx___x___x___ ,
 /*ECDE*/ GENx___x___x___ ,
 /*ECDF*/ GENx___x___x___ ,
 /*ECE0*/ GENx___x___x___ ,
 /*ECE1*/ GENx___x___x___ ,
 /*ECE2*/ GENx___x___x___ ,
 /*ECE3*/ GENx___x___x___ ,
 /*ECE4*/ GENx___x___x___ ,
 /*ECE5*/ GENx___x___x___ ,
 /*ECE6*/ GENx___x___x___ ,
 /*ECE7*/ GENx___x___x___ ,
 /*ECE8*/ GENx___x___x___ ,
 /*ECE9*/ GENx___x___x___ ,
 /*ECEA*/ GENx___x___x___ ,
 /*ECEB*/ GENx___x___x___ ,
 /*ECEC*/ GENx___x___x___ ,
 /*ECED*/ GENx___x___x___ ,
 /*ECEE*/ GENx___x___x___ ,
 /*ECEF*/ GENx___x___x___ ,
 /*ECF0*/ GENx___x___x___ ,
 /*ECF1*/ GENx___x___x___ ,
 /*ECF2*/ GENx___x___x___ ,
 /*ECF3*/ GENx___x___x___ ,
 /*ECF4*/ GENx___x___x___ ,
 /*ECF5*/ GENx___x___x___ ,
 /*ECF6*/ GENx___x___x___ ,
 /*ECF7*/ GENx___x___x___ ,
 /*ECF8*/ GENx___x___x___ ,
 /*ECF9*/ GENx___x___x___ ,
 /*ECFA*/ GENx___x___x___ ,
 /*ECFB*/ GENx___x___x___ ,
 /*ECFC*/ GENx___x___x___ ,
 /*ECFD*/ GENx___x___x___ ,
 /*ECFE*/ GENx___x___x___ ,
 /*ECFF*/ GENx___x___x___  };

// #endif /*defined(FEATURE_ESAME)*/

// #if defined(FEATURE_BASIC_FP_EXTENSIONS)

zz_func opcode_edxx[256][GEN_MAXARCH] = {
 /*ED00*/ GENx___x___x___ ,
 /*ED01*/ GENx___x___x___ ,
 /*ED02*/ GENx___x___x___ ,
 /*ED03*/ GENx___x___x___ ,
 /*ED04*/ GENx___x390x900 (loadlength_bfp_short_to_long,RXE,"LDEB"),
 /*ED05*/ GENx___x___x___ ,
 /*ED06*/ GENx___x___x___ ,
 /*ED07*/ GENx___x___x___ ,
 /*ED08*/ GENx___x390x900 (compare_and_signal_bfp_short,RXE,"KEB"),
 /*ED09*/ GENx___x390x900 (compare_bfp_short,RXE,"CEB"),
 /*ED0A*/ GENx___x390x900 (add_bfp_short,RXE,"AEB"),
 /*ED0B*/ GENx___x390x900 (subtract_bfp_short,RXE,"SEB"),
 /*ED0C*/ GENx___x___x___ ,
 /*ED0D*/ GENx___x390x900 (divide_bfp_short,RXE,"DEB"),
 /*ED0E*/ GENx___x___x___ ,
 /*ED0F*/ GENx___x___x___ ,
 /*ED10*/ GENx___x390x900 (testdataclass_bfp_short,RXE,"TCEB"),
 /*ED11*/ GENx___x390x900 (testdataclass_bfp_long,RXE,"TCDB"),
 /*ED12*/ GENx___x390x900 (testdataclass_bfp_ext,RXE,"TCXB"),
 /*ED13*/ GENx___x___x___ ,
 /*ED14*/ GENx___x390x900 (squareroot_bfp_short,RXE,"SQEB"),
 /*ED15*/ GENx___x390x900 (squareroot_bfp_long,RXE,"SQDB"),
 /*ED16*/ GENx___x___x___ ,
 /*ED17*/ GENx___x390x900 (multiply_bfp_short,RXE,"MEEB"),
 /*ED18*/ GENx___x390x900 (compare_and_signal_bfp_long,RXE,"KDB"),
 /*ED19*/ GENx___x390x900 (compare_bfp_long,RXE,"CDB"),
 /*ED1A*/ GENx___x390x900 (add_bfp_long,RXE,"ADB"),
 /*ED1B*/ GENx___x390x900 (subtract_bfp_long,RXE,"SDB"),
 /*ED1C*/ GENx___x390x900 (multiply_bfp_long,RXE,"MDB"),
 /*ED1D*/ GENx___x390x900 (divide_bfp_long,RXE,"DDB"),
 /*ED1E*/ GENx___x___x___ ,
 /*ED1F*/ GENx___x___x___ ,
 /*ED20*/ GENx___x___x___ ,
 /*ED21*/ GENx___x___x___ ,
 /*ED22*/ GENx___x___x___ ,
 /*ED23*/ GENx___x___x___ ,
 /*ED24*/ GENx___x390x900 (loadlength_float_short_to_long,RXE,"LDE"),
 /*ED25*/ GENx___x390x900 (loadlength_float_long_to_ext,RXE,"LXD"),
 /*ED26*/ GENx___x390x900 (loadlength_float_short_to_ext,RXE,"LXE"),
 /*ED27*/ GENx___x___x___ ,
 /*ED28*/ GENx___x___x___ ,
 /*ED29*/ GENx___x___x___ ,
 /*ED2A*/ GENx___x___x___ ,
 /*ED2B*/ GENx___x___x___ ,
 /*ED2C*/ GENx___x___x___ ,
 /*ED2D*/ GENx___x___x___ ,
 /*ED2E*/ GENx___x___x___ ,
 /*ED2F*/ GENx___x___x___ ,
 /*ED30*/ GENx___x___x___ ,
 /*ED31*/ GENx___x___x___ ,
 /*ED32*/ GENx___x___x___ ,
 /*ED33*/ GENx___x___x___ ,
 /*ED34*/ GENx___x390x900 (squareroot_float_short,RXE,"SQE"),
 /*ED35*/ GENx___x390x900 (squareroot_float_long,RXE,"SQD"),
 /*ED36*/ GENx___x___x___ ,
 /*ED37*/ GENx___x390x900 (multiply_float_short,RXE,"MEE"),
 /*ED38*/ GENx___x___x___ ,
 /*ED39*/ GENx___x___x___ ,
 /*ED3A*/ GENx___x___x___ ,
 /*ED3B*/ GENx___x___x___ ,
 /*ED3C*/ GENx___x___x___ ,
 /*ED3D*/ GENx___x___x___ ,
 /*ED3E*/ GENx___x___x___ ,
 /*ED3F*/ GENx___x___x___ ,
 /*ED40*/ GENx___x___x___ ,
 /*ED41*/ GENx___x___x___ ,
 /*ED42*/ GENx___x___x___ ,
 /*ED43*/ GENx___x___x___ ,
 /*ED44*/ GENx___x___x___ ,
 /*ED45*/ GENx___x___x___ ,
 /*ED46*/ GENx___x___x___ ,
 /*ED47*/ GENx___x___x___ ,
 /*ED48*/ GENx___x___x___ ,
 /*ED49*/ GENx___x___x___ ,
 /*ED4A*/ GENx___x___x___ ,
 /*ED4B*/ GENx___x___x___ ,
 /*ED4C*/ GENx___x___x___ ,
 /*ED4D*/ GENx___x___x___ ,
 /*ED4E*/ GENx___x___x___ ,
 /*ED4F*/ GENx___x___x___ ,
 /*ED50*/ GENx___x___x___ ,
 /*ED51*/ GENx___x___x___ ,
 /*ED52*/ GENx___x___x___ ,
 /*ED53*/ GENx___x___x___ ,
 /*ED54*/ GENx___x___x___ ,
 /*ED55*/ GENx___x___x___ ,
 /*ED56*/ GENx___x___x___ ,
 /*ED57*/ GENx___x___x___ ,
 /*ED58*/ GENx___x___x___ ,
 /*ED59*/ GENx___x___x___ ,
 /*ED5A*/ GENx___x___x___ ,
 /*ED5B*/ GENx___x___x___ ,
 /*ED5C*/ GENx___x___x___ ,
 /*ED5D*/ GENx___x___x___ ,
 /*ED5E*/ GENx___x___x___ ,
 /*ED5F*/ GENx___x___x___ ,
 /*ED60*/ GENx___x___x___ ,
 /*ED61*/ GENx___x___x___ ,
 /*ED62*/ GENx___x___x___ ,
 /*ED63*/ GENx___x___x___ ,
 /*ED64*/ GENx___x___x___ ,
 /*ED65*/ GENx___x___x___ ,
 /*ED66*/ GENx___x___x___ ,
 /*ED67*/ GENx___x___x___ ,
 /*ED68*/ GENx___x___x___ ,
 /*ED69*/ GENx___x___x___ ,
 /*ED6A*/ GENx___x___x___ ,
 /*ED6B*/ GENx___x___x___ ,
 /*ED6C*/ GENx___x___x___ ,
 /*ED6D*/ GENx___x___x___ ,
 /*ED6E*/ GENx___x___x___ ,
 /*ED6F*/ GENx___x___x___ ,
 /*ED70*/ GENx___x___x___ ,
 /*ED71*/ GENx___x___x___ ,
 /*ED72*/ GENx___x___x___ ,
 /*ED73*/ GENx___x___x___ ,
 /*ED74*/ GENx___x___x___ ,
 /*ED75*/ GENx___x___x___ ,
 /*ED76*/ GENx___x___x___ ,
 /*ED77*/ GENx___x___x___ ,
 /*ED78*/ GENx___x___x___ ,
 /*ED79*/ GENx___x___x___ ,
 /*ED7A*/ GENx___x___x___ ,
 /*ED7B*/ GENx___x___x___ ,
 /*ED7C*/ GENx___x___x___ ,
 /*ED7D*/ GENx___x___x___ ,
 /*ED7E*/ GENx___x___x___ ,
 /*ED7F*/ GENx___x___x___ ,
 /*ED80*/ GENx___x___x___ ,
 /*ED81*/ GENx___x___x___ ,
 /*ED82*/ GENx___x___x___ ,
 /*ED83*/ GENx___x___x___ ,
 /*ED84*/ GENx___x___x___ ,
 /*ED85*/ GENx___x___x___ ,
 /*ED86*/ GENx___x___x___ ,
 /*ED87*/ GENx___x___x___ ,
 /*ED88*/ GENx___x___x___ ,
 /*ED89*/ GENx___x___x___ ,
 /*ED8A*/ GENx___x___x___ ,
 /*ED8B*/ GENx___x___x___ ,
 /*ED8C*/ GENx___x___x___ ,
 /*ED8D*/ GENx___x___x___ ,
 /*ED8E*/ GENx___x___x___ ,
 /*ED8F*/ GENx___x___x___ ,
 /*ED90*/ GENx___x___x___ ,
 /*ED91*/ GENx___x___x___ ,
 /*ED92*/ GENx___x___x___ ,
 /*ED93*/ GENx___x___x___ ,
 /*ED94*/ GENx___x___x___ ,
 /*ED95*/ GENx___x___x___ ,
 /*ED96*/ GENx___x___x___ ,
 /*ED97*/ GENx___x___x___ ,
 /*ED98*/ GENx___x___x___ ,
 /*ED99*/ GENx___x___x___ ,
 /*ED9A*/ GENx___x___x___ ,
 /*ED9B*/ GENx___x___x___ ,
 /*ED9C*/ GENx___x___x___ ,
 /*ED9D*/ GENx___x___x___ ,
 /*ED9E*/ GENx___x___x___ ,
 /*ED9F*/ GENx___x___x___ ,
 /*EDA0*/ GENx___x___x___ ,
 /*EDA1*/ GENx___x___x___ ,
 /*EDA2*/ GENx___x___x___ ,
 /*EDA3*/ GENx___x___x___ ,
 /*EDA4*/ GENx___x___x___ ,
 /*EDA5*/ GENx___x___x___ ,
 /*EDA6*/ GENx___x___x___ ,
 /*EDA7*/ GENx___x___x___ ,
 /*EDA8*/ GENx___x___x___ ,
 /*EDA9*/ GENx___x___x___ ,
 /*EDAA*/ GENx___x___x___ ,
 /*EDAB*/ GENx___x___x___ ,
 /*EDAC*/ GENx___x___x___ ,
 /*EDAD*/ GENx___x___x___ ,
 /*EDAE*/ GENx___x___x___ ,
 /*EDAF*/ GENx___x___x___ ,
 /*EDB0*/ GENx___x___x___ ,
 /*EDB1*/ GENx___x___x___ ,
 /*EDB2*/ GENx___x___x___ ,
 /*EDB3*/ GENx___x___x___ ,
 /*EDB4*/ GENx___x___x___ ,
 /*EDB5*/ GENx___x___x___ ,
 /*EDB6*/ GENx___x___x___ ,
 /*EDB7*/ GENx___x___x___ ,
 /*EDB8*/ GENx___x___x___ ,
 /*EDB3*/ GENx___x___x___ ,
 /*EDBA*/ GENx___x___x___ ,
 /*EDBB*/ GENx___x___x___ ,
 /*EDBC*/ GENx___x___x___ ,
 /*EDBD*/ GENx___x___x___ ,
 /*EDBE*/ GENx___x___x___ ,
 /*EDBF*/ GENx___x___x___ ,
 /*EDC0*/ GENx___x___x___ ,
 /*EDC1*/ GENx___x___x___ ,
 /*EDC2*/ GENx___x___x___ ,
 /*EDC3*/ GENx___x___x___ ,
 /*EDC4*/ GENx___x___x___ ,
 /*EDC5*/ GENx___x___x___ ,
 /*EDC6*/ GENx___x___x___ ,
 /*EDC7*/ GENx___x___x___ ,
 /*EDC8*/ GENx___x___x___ ,
 /*EDC9*/ GENx___x___x___ ,
 /*EDCA*/ GENx___x___x___ ,
 /*EDCB*/ GENx___x___x___ ,
 /*EDCC*/ GENx___x___x___ ,
 /*EDCD*/ GENx___x___x___ ,
 /*EDCE*/ GENx___x___x___ ,
 /*EDCF*/ GENx___x___x___ ,
 /*EDD0*/ GENx___x___x___ ,
 /*EDD1*/ GENx___x___x___ ,
 /*EDD2*/ GENx___x___x___ ,
 /*EDD3*/ GENx___x___x___ ,
 /*EDD4*/ GENx___x___x___ ,
 /*EDD5*/ GENx___x___x___ ,
 /*EDD6*/ GENx___x___x___ ,
 /*EDD7*/ GENx___x___x___ ,
 /*EDD8*/ GENx___x___x___ ,
 /*EDD9*/ GENx___x___x___ ,
 /*EDDA*/ GENx___x___x___ ,
 /*EDDB*/ GENx___x___x___ ,
 /*EDDC*/ GENx___x___x___ ,
 /*EDDD*/ GENx___x___x___ ,
 /*EDDE*/ GENx___x___x___ ,
 /*EDDF*/ GENx___x___x___ ,
 /*EDE0*/ GENx___x___x___ ,
 /*EDE1*/ GENx___x___x___ ,
 /*EDE2*/ GENx___x___x___ ,
 /*EDE3*/ GENx___x___x___ ,
 /*EDE4*/ GENx___x___x___ ,
 /*EDE5*/ GENx___x___x___ ,
 /*EDE6*/ GENx___x___x___ ,
 /*EDE7*/ GENx___x___x___ ,
 /*EDE8*/ GENx___x___x___ ,
 /*EDE9*/ GENx___x___x___ ,
 /*EDEA*/ GENx___x___x___ ,
 /*EDEB*/ GENx___x___x___ ,
 /*EDEC*/ GENx___x___x___ ,
 /*EDED*/ GENx___x___x___ ,
 /*EDEE*/ GENx___x___x___ ,
 /*EDEF*/ GENx___x___x___ ,
 /*EDF0*/ GENx___x___x___ ,
 /*EDF1*/ GENx___x___x___ ,
 /*EDF2*/ GENx___x___x___ ,
 /*EDF3*/ GENx___x___x___ ,
 /*EDF4*/ GENx___x___x___ ,
 /*EDF5*/ GENx___x___x___ ,
 /*EDF6*/ GENx___x___x___ ,
 /*EDF7*/ GENx___x___x___ ,
 /*EDF8*/ GENx___x___x___ ,
 /*EDF9*/ GENx___x___x___ ,
 /*EDFA*/ GENx___x___x___ ,
 /*EDFB*/ GENx___x___x___ ,
 /*EDFC*/ GENx___x___x___ ,
 /*EDFD*/ GENx___x___x___ ,
 /*EDFE*/ GENx___x___x___ ,
 /*EDFF*/ GENx___x___x___  };

// #endif /*defined(FEATURE_BASIC_FP_EXTENSIONS)*/

// #if defined (FEATURE_VECTOR_FACILITY)

zz_func v_opcode_a4xx[256][GEN_MAXARCH] = {
 /*A400*/ GENx___x___x___ ,
 /*A401*/ GENx___x___x___ ,
 /*A402*/ GENx___x___x___ ,
 /*A403*/ GENx___x___x___ ,
 /*A404*/ GENx___x___x___ ,
 /*A405*/ GENx___x___x___ ,
 /*A406*/ GENx___x___x___ ,
 /*A407*/ GENx___x___x___ ,
 /*A408*/ GENx___x___x___ ,
 /*A409*/ GENx___x___x___ ,
 /*A40A*/ GENx___x___x___ ,
 /*A40B*/ GENx___x___x___ ,
 /*A40C*/ GENx___x___x___ ,
 /*A40D*/ GENx___x___x___ ,
 /*A40E*/ GENx___x___x___ ,
 /*A40F*/ GENx___x___x___ ,
 /*A410*/ GENx___x___x___ ,
 /*A411*/ GENx___x___x___ ,
 /*A412*/ GENx___x___x___ ,
 /*A413*/ GENx___x___x___ ,
 /*A414*/ GENx___x___x___ ,
 /*A415*/ GENx___x___x___ ,
 /*A416*/ GENx___x___x___ ,
 /*A417*/ GENx___x___x___ ,
 /*A418*/ GENx___x___x___ ,
 /*A419*/ GENx___x___x___ ,
 /*A41A*/ GENx___x___x___ ,
 /*A41B*/ GENx___x___x___ ,
 /*A41C*/ GENx___x___x___ ,
 /*A41D*/ GENx___x___x___ ,
 /*A41E*/ GENx___x___x___ ,
 /*A41F*/ GENx___x___x___ ,
 /*A420*/ GENx___x___x___ ,
 /*A421*/ GENx___x___x___ ,
 /*A422*/ GENx___x___x___ ,
 /*A423*/ GENx___x___x___ ,
 /*A424*/ GENx___x___x___ ,
 /*A425*/ GENx___x___x___ ,
 /*A426*/ GENx___x___x___ ,
 /*A427*/ GENx___x___x___ ,
 /*A428*/ GENx___x___x___ ,
 /*A429*/ GENx___x___x___ ,
 /*A42A*/ GENx___x___x___ ,
 /*A42B*/ GENx___x___x___ ,
 /*A42C*/ GENx___x___x___ ,
 /*A42D*/ GENx___x___x___ ,
 /*A42E*/ GENx___x___x___ ,
 /*A42F*/ GENx___x___x___ ,
 /*A430*/ GENx___x___x___ ,
 /*A431*/ GENx___x___x___ ,
 /*A432*/ GENx___x___x___ ,
 /*A433*/ GENx___x___x___ ,
 /*A434*/ GENx___x___x___ ,
 /*A435*/ GENx___x___x___ ,
 /*A436*/ GENx___x___x___ ,
 /*A437*/ GENx___x___x___ ,
 /*A438*/ GENx___x___x___ ,
 /*A439*/ GENx___x___x___ ,
 /*A43A*/ GENx___x___x___ ,
 /*A43B*/ GENx___x___x___ ,
 /*A43C*/ GENx___x___x___ ,
 /*A43D*/ GENx___x___x___ ,
 /*A43E*/ GENx___x___x___ ,
 /*A43F*/ GENx___x___x___ ,
 /*A440*/ GENx___x___x___ ,
 /*A441*/ GENx___x___x___ ,
 /*A442*/ GENx___x___x___ ,
 /*A443*/ GENx___x___x___ ,
 /*A444*/ GENx___x___x___ ,
 /*A445*/ GENx___x___x___ ,
 /*A446*/ GENx___x___x___ ,
 /*A447*/ GENx___x___x___ ,
 /*A448*/ GENx___x___x___ ,
 /*A449*/ GENx___x___x___ ,
 /*A44A*/ GENx___x___x___ ,
 /*A44B*/ GENx___x___x___ ,
 /*A44C*/ GENx___x___x___ ,
 /*A44D*/ GENx___x___x___ ,
 /*A44E*/ GENx___x___x___ ,
 /*A44F*/ GENx___x___x___ ,
 /*A450*/ GENx___x___x___ ,
 /*A451*/ GENx___x___x___ ,
 /*A452*/ GENx___x___x___ ,
 /*A453*/ GENx___x___x___ ,
 /*A454*/ GENx___x___x___ ,
 /*A455*/ GENx___x___x___ ,
 /*A456*/ GENx___x___x___ ,
 /*A457*/ GENx___x___x___ ,
 /*A458*/ GENx___x___x___ ,
 /*A459*/ GENx___x___x___ ,
 /*A45A*/ GENx___x___x___ ,
 /*A45B*/ GENx___x___x___ ,
 /*A45C*/ GENx___x___x___ ,
 /*A45D*/ GENx___x___x___ ,
 /*A45E*/ GENx___x___x___ ,
 /*A45F*/ GENx___x___x___ ,
 /*A460*/ GENx___x___x___ ,
 /*A461*/ GENx___x___x___ ,
 /*A462*/ GENx___x___x___ ,
 /*A463*/ GENx___x___x___ ,
 /*A464*/ GENx___x___x___ ,
 /*A465*/ GENx___x___x___ ,
 /*A466*/ GENx___x___x___ ,
 /*A467*/ GENx___x___x___ ,
 /*A468*/ GENx___x___x___ ,
 /*A469*/ GENx___x___x___ ,
 /*A46A*/ GENx___x___x___ ,
 /*A46B*/ GENx___x___x___ ,
 /*A46C*/ GENx___x___x___ ,
 /*A46D*/ GENx___x___x___ ,
 /*A46E*/ GENx___x___x___ ,
 /*A46F*/ GENx___x___x___ ,
 /*A470*/ GENx___x___x___ ,
 /*A471*/ GENx___x___x___ ,
 /*A472*/ GENx___x___x___ ,
 /*A473*/ GENx___x___x___ ,
 /*A474*/ GENx___x___x___ ,
 /*A475*/ GENx___x___x___ ,
 /*A476*/ GENx___x___x___ ,
 /*A477*/ GENx___x___x___ ,
 /*A478*/ GENx___x___x___ ,
 /*A479*/ GENx___x___x___ ,
 /*A47A*/ GENx___x___x___ ,
 /*A47B*/ GENx___x___x___ ,
 /*A47C*/ GENx___x___x___ ,
 /*A47D*/ GENx___x___x___ ,
 /*A47E*/ GENx___x___x___ ,
 /*A47F*/ GENx___x___x___ ,
 /*A480*/ GENx___x___x___ ,
 /*A481*/ GENx___x___x___ ,
 /*A482*/ GENx___x___x___ ,
 /*A483*/ GENx___x___x___ ,
 /*A484*/ GENx___x___x___ ,
 /*A485*/ GENx___x___x___ ,
 /*A486*/ GENx___x___x___ ,
 /*A487*/ GENx___x___x___ ,
 /*A488*/ GENx___x___x___ ,
 /*A489*/ GENx___x___x___ ,
 /*A48A*/ GENx___x___x___ ,
 /*A48B*/ GENx___x___x___ ,
 /*A48C*/ GENx___x___x___ ,
 /*A48D*/ GENx___x___x___ ,
 /*A48E*/ GENx___x___x___ ,
 /*A48F*/ GENx___x___x___ ,
 /*A490*/ GENx___x___x___ ,
 /*A491*/ GENx___x___x___ ,
 /*A492*/ GENx___x___x___ ,
 /*A493*/ GENx___x___x___ ,
 /*A494*/ GENx___x___x___ ,
 /*A495*/ GENx___x___x___ ,
 /*A496*/ GENx___x___x___ ,
 /*A497*/ GENx___x___x___ ,
 /*A498*/ GENx___x___x___ ,
 /*A499*/ GENx___x___x___ ,
 /*A49A*/ GENx___x___x___ ,
 /*A49B*/ GENx___x___x___ ,
 /*A49C*/ GENx___x___x___ ,
 /*A49D*/ GENx___x___x___ ,
 /*A49E*/ GENx___x___x___ ,
 /*A49F*/ GENx___x___x___ ,
 /*A4A0*/ GENx___x___x___ ,
 /*A4A1*/ GENx___x___x___ ,
 /*A4A2*/ GENx___x___x___ ,
 /*A4A3*/ GENx___x___x___ ,
 /*A4A4*/ GENx___x___x___ ,
 /*A4A5*/ GENx___x___x___ ,
 /*A4A6*/ GENx___x___x___ ,
 /*A4A7*/ GENx___x___x___ ,
 /*A4A8*/ GENx___x___x___ ,
 /*A4A9*/ GENx___x___x___ ,
 /*A4AA*/ GENx___x___x___ ,
 /*A4AB*/ GENx___x___x___ ,
 /*A4AC*/ GENx___x___x___ ,
 /*A4AD*/ GENx___x___x___ ,
 /*A4AE*/ GENx___x___x___ ,
 /*A4AF*/ GENx___x___x___ ,
 /*A4B0*/ GENx___x___x___ ,
 /*A4B1*/ GENx___x___x___ ,
 /*A4B2*/ GENx___x___x___ ,
 /*A4B3*/ GENx___x___x___ ,
 /*A4B4*/ GENx___x___x___ ,
 /*A4B5*/ GENx___x___x___ ,
 /*A4B6*/ GENx___x___x___ ,
 /*A4B7*/ GENx___x___x___ ,
 /*A4B8*/ GENx___x___x___ ,
 /*A4B9*/ GENx___x___x___ ,
 /*A4BA*/ GENx___x___x___ ,
 /*A4BB*/ GENx___x___x___ ,
 /*A4BC*/ GENx___x___x___ ,
 /*A4BD*/ GENx___x___x___ ,
 /*A4BE*/ GENx___x___x___ ,
 /*A4BF*/ GENx___x___x___ ,
 /*A4C0*/ GENx___x___x___ ,
 /*A4C1*/ GENx___x___x___ ,
 /*A4C2*/ GENx___x___x___ ,
 /*A4C3*/ GENx___x___x___ ,
 /*A4C4*/ GENx___x___x___ ,
 /*A4C5*/ GENx___x___x___ ,
 /*A4C6*/ GENx___x___x___ ,
 /*A4C7*/ GENx___x___x___ ,
 /*A4C8*/ GENx___x___x___ ,
 /*A4C9*/ GENx___x___x___ ,
 /*A4CA*/ GENx___x___x___ ,
 /*A4CB*/ GENx___x___x___ ,
 /*A4CC*/ GENx___x___x___ ,
 /*A4CD*/ GENx___x___x___ ,
 /*A4CE*/ GENx___x___x___ ,
 /*A4CF*/ GENx___x___x___ ,
 /*A4D0*/ GENx___x___x___ ,
 /*A4D1*/ GENx___x___x___ ,
 /*A4D2*/ GENx___x___x___ ,
 /*A4D3*/ GENx___x___x___ ,
 /*A4D4*/ GENx___x___x___ ,
 /*A4D5*/ GENx___x___x___ ,
 /*A4D6*/ GENx___x___x___ ,
 /*A4D7*/ GENx___x___x___ ,
 /*A4D8*/ GENx___x___x___ ,
 /*A4D9*/ GENx___x___x___ ,
 /*A4DA*/ GENx___x___x___ ,
 /*A4DB*/ GENx___x___x___ ,
 /*A4DC*/ GENx___x___x___ ,
 /*A4DD*/ GENx___x___x___ ,
 /*A4DE*/ GENx___x___x___ ,
 /*A4DF*/ GENx___x___x___ ,
 /*A4E0*/ GENx___x___x___ ,
 /*A4E1*/ GENx___x___x___ ,
 /*A4E2*/ GENx___x___x___ ,
 /*A4E3*/ GENx___x___x___ ,
 /*A4E4*/ GENx___x___x___ ,
 /*A4E5*/ GENx___x___x___ ,
 /*A4E6*/ GENx___x___x___ ,
 /*A4E7*/ GENx___x___x___ ,
 /*A4E8*/ GENx___x___x___ ,
 /*A4E9*/ GENx___x___x___ ,
 /*A4EA*/ GENx___x___x___ ,
 /*A4EB*/ GENx___x___x___ ,
 /*A4EC*/ GENx___x___x___ ,
 /*A4ED*/ GENx___x___x___ ,
 /*A4EE*/ GENx___x___x___ ,
 /*A4EF*/ GENx___x___x___ ,
 /*A4F0*/ GENx___x___x___ ,
 /*A4F1*/ GENx___x___x___ ,
 /*A4F2*/ GENx___x___x___ ,
 /*A4F3*/ GENx___x___x___ ,
 /*A4F4*/ GENx___x___x___ ,
 /*A4F5*/ GENx___x___x___ ,
 /*A4F6*/ GENx___x___x___ ,
 /*A4F7*/ GENx___x___x___ ,
 /*A4F8*/ GENx___x___x___ ,
 /*A4F9*/ GENx___x___x___ ,
 /*A4FA*/ GENx___x___x___ ,
 /*A4FB*/ GENx___x___x___ ,
 /*A4FC*/ GENx___x___x___ ,
 /*A4FD*/ GENx___x___x___ ,
 /*A4FE*/ GENx___x___x___ ,
 /*A4FF*/ GENx___x___x___  };

// #endif /*defined (FEATURE_VECTOR_FACILITY)*/

// #if defined (FEATURE_VECTOR_FACILITY)

zz_func v_opcode_a5xx[256][GEN_MAXARCH] = {
 /*A500*/ GENx___x___x___ ,
 /*A501*/ GENx___x___x___ ,
 /*A502*/ GENx___x___x___ ,
 /*A503*/ GENx___x___x___ ,
 /*A504*/ GENx___x___x___ ,
 /*A505*/ GENx___x___x___ ,
 /*A506*/ GENx___x___x___ ,
 /*A507*/ GENx___x___x___ ,
 /*A508*/ GENx___x___x___ ,
 /*A509*/ GENx___x___x___ ,
 /*A50A*/ GENx___x___x___ ,
 /*A50B*/ GENx___x___x___ ,
 /*A50C*/ GENx___x___x___ ,
 /*A50D*/ GENx___x___x___ ,
 /*A50E*/ GENx___x___x___ ,
 /*A50F*/ GENx___x___x___ ,
 /*A510*/ GENx___x___x___ ,
 /*A511*/ GENx___x___x___ ,
 /*A512*/ GENx___x___x___ ,
 /*A513*/ GENx___x___x___ ,
 /*A514*/ GENx___x___x___ ,
 /*A515*/ GENx___x___x___ ,
 /*A516*/ GENx___x___x___ ,
 /*A517*/ GENx___x___x___ ,
 /*A518*/ GENx___x___x___ ,
 /*A519*/ GENx___x___x___ ,
 /*A51A*/ GENx___x___x___ ,
 /*A51B*/ GENx___x___x___ ,
 /*A51C*/ GENx___x___x___ ,
 /*A51D*/ GENx___x___x___ ,
 /*A51E*/ GENx___x___x___ ,
 /*A51F*/ GENx___x___x___ ,
 /*A520*/ GENx___x___x___ ,
 /*A521*/ GENx___x___x___ ,
 /*A522*/ GENx___x___x___ ,
 /*A523*/ GENx___x___x___ ,
 /*A524*/ GENx___x___x___ ,
 /*A525*/ GENx___x___x___ ,
 /*A526*/ GENx___x___x___ ,
 /*A527*/ GENx___x___x___ ,
 /*A528*/ GENx___x___x___ ,
 /*A529*/ GENx___x___x___ ,
 /*A52A*/ GENx___x___x___ ,
 /*A52B*/ GENx___x___x___ ,
 /*A52C*/ GENx___x___x___ ,
 /*A52D*/ GENx___x___x___ ,
 /*A52E*/ GENx___x___x___ ,
 /*A52F*/ GENx___x___x___ ,
 /*A530*/ GENx___x___x___ ,
 /*A531*/ GENx___x___x___ ,
 /*A532*/ GENx___x___x___ ,
 /*A533*/ GENx___x___x___ ,
 /*A534*/ GENx___x___x___ ,
 /*A535*/ GENx___x___x___ ,
 /*A536*/ GENx___x___x___ ,
 /*A537*/ GENx___x___x___ ,
 /*A538*/ GENx___x___x___ ,
 /*A539*/ GENx___x___x___ ,
 /*A53A*/ GENx___x___x___ ,
 /*A53B*/ GENx___x___x___ ,
 /*A53C*/ GENx___x___x___ ,
 /*A53D*/ GENx___x___x___ ,
 /*A53E*/ GENx___x___x___ ,
 /*A53F*/ GENx___x___x___ ,
 /*A540*/ GENx___x___x___ ,
 /*A541*/ GENx___x___x___ ,
 /*A542*/ GENx___x___x___ ,
 /*A543*/ GENx___x___x___ ,
 /*A544*/ GENx___x___x___ ,
 /*A545*/ GENx___x___x___ ,
 /*A546*/ GENx___x___x___ ,
 /*A547*/ GENx___x___x___ ,
 /*A548*/ GENx___x___x___ ,
 /*A549*/ GENx___x___x___ ,
 /*A54A*/ GENx___x___x___ ,
 /*A54B*/ GENx___x___x___ ,
 /*A54C*/ GENx___x___x___ ,
 /*A54D*/ GENx___x___x___ ,
 /*A54E*/ GENx___x___x___ ,
 /*A54F*/ GENx___x___x___ ,
 /*A550*/ GENx___x___x___ ,
 /*A551*/ GENx___x___x___ ,
 /*A552*/ GENx___x___x___ ,
 /*A553*/ GENx___x___x___ ,
 /*A554*/ GENx___x___x___ ,
 /*A555*/ GENx___x___x___ ,
 /*A556*/ GENx___x___x___ ,
 /*A557*/ GENx___x___x___ ,
 /*A558*/ GENx___x___x___ ,
 /*A559*/ GENx___x___x___ ,
 /*A55A*/ GENx___x___x___ ,
 /*A55B*/ GENx___x___x___ ,
 /*A55C*/ GENx___x___x___ ,
 /*A55D*/ GENx___x___x___ ,
 /*A55E*/ GENx___x___x___ ,
 /*A55F*/ GENx___x___x___ ,
 /*A560*/ GENx___x___x___ ,
 /*A561*/ GENx___x___x___ ,
 /*A562*/ GENx___x___x___ ,
 /*A563*/ GENx___x___x___ ,
 /*A564*/ GENx___x___x___ ,
 /*A565*/ GENx___x___x___ ,
 /*A566*/ GENx___x___x___ ,
 /*A567*/ GENx___x___x___ ,
 /*A568*/ GENx___x___x___ ,
 /*A569*/ GENx___x___x___ ,
 /*A56A*/ GENx___x___x___ ,
 /*A56B*/ GENx___x___x___ ,
 /*A56C*/ GENx___x___x___ ,
 /*A56D*/ GENx___x___x___ ,
 /*A56E*/ GENx___x___x___ ,
 /*A56F*/ GENx___x___x___ ,
 /*A570*/ GENx___x___x___ ,
 /*A571*/ GENx___x___x___ ,
 /*A572*/ GENx___x___x___ ,
 /*A573*/ GENx___x___x___ ,
 /*A574*/ GENx___x___x___ ,
 /*A575*/ GENx___x___x___ ,
 /*A576*/ GENx___x___x___ ,
 /*A577*/ GENx___x___x___ ,
 /*A578*/ GENx___x___x___ ,
 /*A579*/ GENx___x___x___ ,
 /*A57A*/ GENx___x___x___ ,
 /*A57B*/ GENx___x___x___ ,
 /*A57C*/ GENx___x___x___ ,
 /*A57D*/ GENx___x___x___ ,
 /*A57E*/ GENx___x___x___ ,
 /*A57F*/ GENx___x___x___ ,
 /*A580*/ GENx___x___x___ ,
 /*A581*/ GENx___x___x___ ,
 /*A582*/ GENx___x___x___ ,
 /*A583*/ GENx___x___x___ ,
 /*A584*/ GENx___x___x___ ,
 /*A585*/ GENx___x___x___ ,
 /*A586*/ GENx___x___x___ ,
 /*A587*/ GENx___x___x___ ,
 /*A588*/ GENx___x___x___ ,
 /*A589*/ GENx___x___x___ ,
 /*A58A*/ GENx___x___x___ ,
 /*A58B*/ GENx___x___x___ ,
 /*A58C*/ GENx___x___x___ ,
 /*A58D*/ GENx___x___x___ ,
 /*A58E*/ GENx___x___x___ ,
 /*A58F*/ GENx___x___x___ ,
 /*A590*/ GENx___x___x___ ,
 /*A591*/ GENx___x___x___ ,
 /*A592*/ GENx___x___x___ ,
 /*A593*/ GENx___x___x___ ,
 /*A594*/ GENx___x___x___ ,
 /*A595*/ GENx___x___x___ ,
 /*A596*/ GENx___x___x___ ,
 /*A597*/ GENx___x___x___ ,
 /*A598*/ GENx___x___x___ ,
 /*A599*/ GENx___x___x___ ,
 /*A59A*/ GENx___x___x___ ,
 /*A59B*/ GENx___x___x___ ,
 /*A59C*/ GENx___x___x___ ,
 /*A59D*/ GENx___x___x___ ,
 /*A59E*/ GENx___x___x___ ,
 /*A59F*/ GENx___x___x___ ,
 /*A5A0*/ GENx___x___x___ ,
 /*A5A1*/ GENx___x___x___ ,
 /*A5A2*/ GENx___x___x___ ,
 /*A5A3*/ GENx___x___x___ ,
 /*A5A4*/ GENx___x___x___ ,
 /*A5A5*/ GENx___x___x___ ,
 /*A5A6*/ GENx___x___x___ ,
 /*A5A7*/ GENx___x___x___ ,
 /*A5A8*/ GENx___x___x___ ,
 /*A5A9*/ GENx___x___x___ ,
 /*A5AA*/ GENx___x___x___ ,
 /*A5AB*/ GENx___x___x___ ,
 /*A5AC*/ GENx___x___x___ ,
 /*A5AD*/ GENx___x___x___ ,
 /*A5AE*/ GENx___x___x___ ,
 /*A5AF*/ GENx___x___x___ ,
 /*A5B0*/ GENx___x___x___ ,
 /*A5B1*/ GENx___x___x___ ,
 /*A5B2*/ GENx___x___x___ ,
 /*A5B3*/ GENx___x___x___ ,
 /*A5B4*/ GENx___x___x___ ,
 /*A5B5*/ GENx___x___x___ ,
 /*A5B6*/ GENx___x___x___ ,
 /*A5B7*/ GENx___x___x___ ,
 /*A5B8*/ GENx___x___x___ ,
 /*A5B9*/ GENx___x___x___ ,
 /*A5BA*/ GENx___x___x___ ,
 /*A5BB*/ GENx___x___x___ ,
 /*A5BC*/ GENx___x___x___ ,
 /*A5BD*/ GENx___x___x___ ,
 /*A5BE*/ GENx___x___x___ ,
 /*A5BF*/ GENx___x___x___ ,
 /*A5C0*/ GENx___x___x___ ,
 /*A5C1*/ GENx___x___x___ ,
 /*A5C2*/ GENx___x___x___ ,
 /*A5C3*/ GENx___x___x___ ,
 /*A5C4*/ GENx___x___x___ ,
 /*A5C5*/ GENx___x___x___ ,
 /*A5C6*/ GENx___x___x___ ,
 /*A5C7*/ GENx___x___x___ ,
 /*A5C8*/ GENx___x___x___ ,
 /*A5C9*/ GENx___x___x___ ,
 /*A5CA*/ GENx___x___x___ ,
 /*A5CB*/ GENx___x___x___ ,
 /*A5CC*/ GENx___x___x___ ,
 /*A5CD*/ GENx___x___x___ ,
 /*A5CE*/ GENx___x___x___ ,
 /*A5CF*/ GENx___x___x___ ,
 /*A5D0*/ GENx___x___x___ ,
 /*A5D1*/ GENx___x___x___ ,
 /*A5D2*/ GENx___x___x___ ,
 /*A5D3*/ GENx___x___x___ ,
 /*A5D4*/ GENx___x___x___ ,
 /*A5D5*/ GENx___x___x___ ,
 /*A5D6*/ GENx___x___x___ ,
 /*A5D7*/ GENx___x___x___ ,
 /*A5D8*/ GENx___x___x___ ,
 /*A5D9*/ GENx___x___x___ ,
 /*A5DA*/ GENx___x___x___ ,
 /*A5DB*/ GENx___x___x___ ,
 /*A5DC*/ GENx___x___x___ ,
 /*A5DD*/ GENx___x___x___ ,
 /*A5DE*/ GENx___x___x___ ,
 /*A5DF*/ GENx___x___x___ ,
 /*A5E0*/ GENx___x___x___ ,
 /*A5E1*/ GENx___x___x___ ,
 /*A5E2*/ GENx___x___x___ ,
 /*A5E3*/ GENx___x___x___ ,
 /*A5E4*/ GENx___x___x___ ,
 /*A5E5*/ GENx___x___x___ ,
 /*A5E6*/ GENx___x___x___ ,
 /*A5E7*/ GENx___x___x___ ,
 /*A5E8*/ GENx___x___x___ ,
 /*A5E9*/ GENx___x___x___ ,
 /*A5EA*/ GENx___x___x___ ,
 /*A5EB*/ GENx___x___x___ ,
 /*A5EC*/ GENx___x___x___ ,
 /*A5ED*/ GENx___x___x___ ,
 /*A5EE*/ GENx___x___x___ ,
 /*A5EF*/ GENx___x___x___ ,
 /*A5F0*/ GENx___x___x___ ,
 /*A5F1*/ GENx___x___x___ ,
 /*A5F2*/ GENx___x___x___ ,
 /*A5F3*/ GENx___x___x___ ,
 /*A5F4*/ GENx___x___x___ ,
 /*A5F5*/ GENx___x___x___ ,
 /*A5F6*/ GENx___x___x___ ,
 /*A5F7*/ GENx___x___x___ ,
 /*A5F8*/ GENx___x___x___ ,
 /*A5F9*/ GENx___x___x___ ,
 /*A5FA*/ GENx___x___x___ ,
 /*A5FB*/ GENx___x___x___ ,
 /*A5FC*/ GENx___x___x___ ,
 /*A5FD*/ GENx___x___x___ ,
 /*A5FE*/ GENx___x___x___ ,
 /*A5FF*/ GENx___x___x___  };

// #endif /*defined (FEATURE_VECTOR_FACILITY)*/

// #if defined (FEATURE_VECTOR_FACILITY)

zz_func v_opcode_a6xx[256][GEN_MAXARCH] = {
 /*A600*/ GENx___x___x___ ,
 /*A601*/ GENx___x___x___ ,
 /*A602*/ GENx___x___x___ ,
 /*A603*/ GENx___x___x___ ,
 /*A604*/ GENx___x___x___ ,
 /*A605*/ GENx___x___x___ ,
 /*A606*/ GENx___x___x___ ,
 /*A607*/ GENx___x___x___ ,
 /*A608*/ GENx___x___x___ ,
 /*A609*/ GENx___x___x___ ,
 /*A60A*/ GENx___x___x___ ,
 /*A60B*/ GENx___x___x___ ,
 /*A60C*/ GENx___x___x___ ,
 /*A60D*/ GENx___x___x___ ,
 /*A60E*/ GENx___x___x___ ,
 /*A60F*/ GENx___x___x___ ,
 /*A610*/ GENx___x___x___ ,
 /*A611*/ GENx___x___x___ ,
 /*A612*/ GENx___x___x___ ,
 /*A613*/ GENx___x___x___ ,
 /*A614*/ GENx___x___x___ ,
 /*A615*/ GENx___x___x___ ,
 /*A616*/ GENx___x___x___ ,
 /*A617*/ GENx___x___x___ ,
 /*A618*/ GENx___x___x___ ,
 /*A619*/ GENx___x___x___ ,
 /*A61A*/ GENx___x___x___ ,
 /*A61B*/ GENx___x___x___ ,
 /*A61C*/ GENx___x___x___ ,
 /*A61D*/ GENx___x___x___ ,
 /*A61E*/ GENx___x___x___ ,
 /*A61F*/ GENx___x___x___ ,
 /*A620*/ GENx___x___x___ ,
 /*A621*/ GENx___x___x___ ,
 /*A622*/ GENx___x___x___ ,
 /*A623*/ GENx___x___x___ ,
 /*A624*/ GENx___x___x___ ,
 /*A625*/ GENx___x___x___ ,
 /*A626*/ GENx___x___x___ ,
 /*A627*/ GENx___x___x___ ,
 /*A628*/ GENx___x___x___ ,
 /*A629*/ GENx___x___x___ ,
 /*A62A*/ GENx___x___x___ ,
 /*A62B*/ GENx___x___x___ ,
 /*A62C*/ GENx___x___x___ ,
 /*A62D*/ GENx___x___x___ ,
 /*A62E*/ GENx___x___x___ ,
 /*A62F*/ GENx___x___x___ ,
 /*A630*/ GENx___x___x___ ,
 /*A631*/ GENx___x___x___ ,
 /*A632*/ GENx___x___x___ ,
 /*A633*/ GENx___x___x___ ,
 /*A634*/ GENx___x___x___ ,
 /*A635*/ GENx___x___x___ ,
 /*A636*/ GENx___x___x___ ,
 /*A637*/ GENx___x___x___ ,
 /*A638*/ GENx___x___x___ ,
 /*A639*/ GENx___x___x___ ,
 /*A63A*/ GENx___x___x___ ,
 /*A63B*/ GENx___x___x___ ,
 /*A63C*/ GENx___x___x___ ,
 /*A63D*/ GENx___x___x___ ,
 /*A63E*/ GENx___x___x___ ,
 /*A63F*/ GENx___x___x___ ,
 /*A640*/ GENx370x390x___ (v_test_vmr,RRE,"VTVM"),
 /*A641*/ GENx370x390x___ (v_complement_vmr,RRE,"VCVM"),
 /*A642*/ GENx370x390x___ (v_count_left_zeros_in_vmr,RRE,"VCZVM"),
 /*A643*/ GENx370x390x___ (v_count_ones_in_vmr,RRE,"VCOVM"),
 /*A644*/ GENx370x390x___ (v_extract_vct,RRE,"VXVC"),
 /*A645*/ GENx___x___x___ ,
 /*A646*/ GENx370x390x___ (v_extract_vector_modes,RRE,"VXVMM"),
 /*A647*/ GENx___x___x___ ,
 /*A648*/ GENx370x390x___ (v_restore_vr,RRE,"VRRS"),
 /*A649*/ GENx370x390x___ (v_save_changed_vr,RRE,"VRSVC"),
 /*A64A*/ GENx370x390x___ (v_save_vr,RRE,"VRSV"),
 /*A64B*/ GENx___x___x___ ,
 /*A64C*/ GENx___x___x___ ,
 /*A64D*/ GENx___x___x___ ,
 /*A64E*/ GENx___x___x___ ,
 /*A64F*/ GENx___x___x___ ,
 /*A650*/ GENx___x___x___ ,
 /*A651*/ GENx___x___x___ ,
 /*A652*/ GENx___x___x___ ,
 /*A653*/ GENx___x___x___ ,
 /*A654*/ GENx___x___x___ ,
 /*A655*/ GENx___x___x___ ,
 /*A656*/ GENx___x___x___ ,
 /*A657*/ GENx___x___x___ ,
 /*A658*/ GENx___x___x___ ,
 /*A659*/ GENx___x___x___ ,
 /*A65A*/ GENx___x___x___ ,
 /*A65B*/ GENx___x___x___ ,
 /*A65C*/ GENx___x___x___ ,
 /*A65D*/ GENx___x___x___ ,
 /*A65E*/ GENx___x___x___ ,
 /*A65F*/ GENx___x___x___ ,
 /*A660*/ GENx___x___x___ ,
 /*A661*/ GENx___x___x___ ,
 /*A662*/ GENx___x___x___ ,
 /*A663*/ GENx___x___x___ ,
 /*A664*/ GENx___x___x___ ,
 /*A665*/ GENx___x___x___ ,
 /*A666*/ GENx___x___x___ ,
 /*A667*/ GENx___x___x___ ,
 /*A668*/ GENx___x___x___ ,
 /*A669*/ GENx___x___x___ ,
 /*A66A*/ GENx___x___x___ ,
 /*A66B*/ GENx___x___x___ ,
 /*A66C*/ GENx___x___x___ ,
 /*A66D*/ GENx___x___x___ ,
 /*A66E*/ GENx___x___x___ ,
 /*A66F*/ GENx___x___x___ ,
 /*A670*/ GENx___x___x___ ,
 /*A671*/ GENx___x___x___ ,
 /*A672*/ GENx___x___x___ ,
 /*A673*/ GENx___x___x___ ,
 /*A674*/ GENx___x___x___ ,
 /*A675*/ GENx___x___x___ ,
 /*A676*/ GENx___x___x___ ,
 /*A677*/ GENx___x___x___ ,
 /*A678*/ GENx___x___x___ ,
 /*A679*/ GENx___x___x___ ,
 /*A67A*/ GENx___x___x___ ,
 /*A67B*/ GENx___x___x___ ,
 /*A67C*/ GENx___x___x___ ,
 /*A67D*/ GENx___x___x___ ,
 /*A67E*/ GENx___x___x___ ,
 /*A67F*/ GENx___x___x___ ,
 /*A680*/ GENx370x390x___ (v_load_vmr,VS,"VLVM"),
 /*A681*/ GENx370x390x___ (v_load_vmr_complement,VS,"VLCVM"),
 /*A682*/ GENx370x390x___ (v_store_vmr,VS,"VSTVM"),
 /*A683*/ GENx___x___x___ ,
 /*A684*/ GENx370x390x___ (v_and_to_vmr,VS,"VNVM"),
 /*A685*/ GENx370x390x___ (v_or_to_vmr,VS,"VOVM"),
 /*A686*/ GENx370x390x___ (v_exclusive_or_to_vmr,VS,"VXVM"),
 /*A687*/ GENx___x___x___ ,
 /*A688*/ GENx___x___x___ ,
 /*A689*/ GENx___x___x___ ,
 /*A68A*/ GENx___x___x___ ,
 /*A68B*/ GENx___x___x___ ,
 /*A68C*/ GENx___x___x___ ,
 /*A68D*/ GENx___x___x___ ,
 /*A68E*/ GENx___x___x___ ,
 /*A68F*/ GENx___x___x___ ,
 /*A690*/ GENx___x___x___ ,
 /*A691*/ GENx___x___x___ ,
 /*A692*/ GENx___x___x___ ,
 /*A693*/ GENx___x___x___ ,
 /*A694*/ GENx___x___x___ ,
 /*A695*/ GENx___x___x___ ,
 /*A696*/ GENx___x___x___ ,
 /*A697*/ GENx___x___x___ ,
 /*A698*/ GENx___x___x___ ,
 /*A699*/ GENx___x___x___ ,
 /*A69A*/ GENx___x___x___ ,
 /*A69B*/ GENx___x___x___ ,
 /*A69C*/ GENx___x___x___ ,
 /*A69D*/ GENx___x___x___ ,
 /*A69E*/ GENx___x___x___ ,
 /*A69F*/ GENx___x___x___ ,
 /*A6A0*/ GENx___x___x___ ,
 /*A6A1*/ GENx___x___x___ ,
 /*A6A2*/ GENx___x___x___ ,
 /*A6A3*/ GENx___x___x___ ,
 /*A6A4*/ GENx___x___x___ ,
 /*A6A5*/ GENx___x___x___ ,
 /*A6A6*/ GENx___x___x___ ,
 /*A6A7*/ GENx___x___x___ ,
 /*A6A8*/ GENx___x___x___ ,
 /*A6A9*/ GENx___x___x___ ,
 /*A6AA*/ GENx___x___x___ ,
 /*A6AB*/ GENx___x___x___ ,
 /*A6AC*/ GENx___x___x___ ,
 /*A6AD*/ GENx___x___x___ ,
 /*A6AE*/ GENx___x___x___ ,
 /*A6AF*/ GENx___x___x___ ,
 /*A6B0*/ GENx___x___x___ ,
 /*A6B1*/ GENx___x___x___ ,
 /*A6B2*/ GENx___x___x___ ,
 /*A6B3*/ GENx___x___x___ ,
 /*A6B4*/ GENx___x___x___ ,
 /*A6B5*/ GENx___x___x___ ,
 /*A6B6*/ GENx___x___x___ ,
 /*A6B7*/ GENx___x___x___ ,
 /*A6B8*/ GENx___x___x___ ,
 /*A6B9*/ GENx___x___x___ ,
 /*A6BA*/ GENx___x___x___ ,
 /*A6BB*/ GENx___x___x___ ,
 /*A6BC*/ GENx___x___x___ ,
 /*A6BD*/ GENx___x___x___ ,
 /*A6BE*/ GENx___x___x___ ,
 /*A6BF*/ GENx___x___x___ ,
 /*A6C0*/ GENx370x390x___ (v_save_vsr,S,"VSRSV"),
 /*A6C1*/ GENx370x390x___ (v_save_vmr,S,"VMRSV"),
 /*A6C2*/ GENx370x390x___ (v_restore_vsr,S,"VSRRS"),
 /*A6C3*/ GENx370x390x___ (v_restore_vmr,S,"VMRRS"),
 /*A6C4*/ GENx370x390x___ (v_load_vct_from_address,S,"VLVCA"),
 /*A6C5*/ GENx370x390x___ (v_clear_vr,S,"VRCL"),
 /*A6C6*/ GENx370x390x___ (v_set_vector_mask_mode,S,"VSVMM"),
 /*A6C7*/ GENx370x390x___ (v_load_vix_from_address,S,"VLVXA"),
 /*A6C8*/ GENx370x390x___ (v_store_vector_parameters,S,"VSTVP"),
 /*A6C9*/ GENx___x___x___ ,
 /*A6CA*/ GENx370x390x___ (v_save_vac,S,"VACSV"),
 /*A6CB*/ GENx370x390x___ (v_restore_vac,S,"VACRS"),
 /*A6CC*/ GENx___x___x___ ,
 /*A6CD*/ GENx___x___x___ ,
 /*A6CE*/ GENx___x___x___ ,
 /*A6CF*/ GENx___x___x___ ,
 /*A6D0*/ GENx___x___x___ ,
 /*A6D1*/ GENx___x___x___ ,
 /*A6D2*/ GENx___x___x___ ,
 /*A6D3*/ GENx___x___x___ ,
 /*A6D4*/ GENx___x___x___ ,
 /*A6D5*/ GENx___x___x___ ,
 /*A6D6*/ GENx___x___x___ ,
 /*A6D7*/ GENx___x___x___ ,
 /*A6D8*/ GENx___x___x___ ,
 /*A6D9*/ GENx___x___x___ ,
 /*A6DA*/ GENx___x___x___ ,
 /*A6DB*/ GENx___x___x___ ,
 /*A6DC*/ GENx___x___x___ ,
 /*A6DD*/ GENx___x___x___ ,
 /*A6DE*/ GENx___x___x___ ,
 /*A6DF*/ GENx___x___x___ ,
 /*A6E0*/ GENx___x___x___ ,
 /*A6E1*/ GENx___x___x___ ,
 /*A6E2*/ GENx___x___x___ ,
 /*A6E3*/ GENx___x___x___ ,
 /*A6E4*/ GENx___x___x___ ,
 /*A6E5*/ GENx___x___x___ ,
 /*A6E6*/ GENx___x___x___ ,
 /*A6E7*/ GENx___x___x___ ,
 /*A6E8*/ GENx___x___x___ ,
 /*A6E9*/ GENx___x___x___ ,
 /*A6EA*/ GENx___x___x___ ,
 /*A6EB*/ GENx___x___x___ ,
 /*A6EC*/ GENx___x___x___ ,
 /*A6ED*/ GENx___x___x___ ,
 /*A6EE*/ GENx___x___x___ ,
 /*A6EF*/ GENx___x___x___ ,
 /*A6F0*/ GENx___x___x___ ,
 /*A6F1*/ GENx___x___x___ ,
 /*A6F2*/ GENx___x___x___ ,
 /*A6F3*/ GENx___x___x___ ,
 /*A6F4*/ GENx___x___x___ ,
 /*A6F5*/ GENx___x___x___ ,
 /*A6F6*/ GENx___x___x___ ,
 /*A6F7*/ GENx___x___x___ ,
 /*A6F8*/ GENx___x___x___ ,
 /*A6F9*/ GENx___x___x___ ,
 /*A6FA*/ GENx___x___x___ ,
 /*A6FB*/ GENx___x___x___ ,
 /*A6FC*/ GENx___x___x___ ,
 /*A6FD*/ GENx___x___x___ ,
 /*A6FE*/ GENx___x___x___ ,
 /*A6FF*/ GENx___x___x___  };

// #endif /*defined (FEATURE_VECTOR_FACILITY)*/


// #if defined (FEATURE_VECTOR_FACILITY)

zz_func v_opcode_e4xx[256][GEN_MAXARCH] = {
 /*E400*/ GENx___x___x___ ,
 /*E401*/ GENx___x___x___ ,
 /*E402*/ GENx___x___x___ ,
 /*E403*/ GENx___x___x___ ,
 /*E404*/ GENx___x___x___ ,
 /*E405*/ GENx___x___x___ ,
 /*E406*/ GENx___x___x___ ,
 /*E407*/ GENx___x___x___ ,
 /*E408*/ GENx___x___x___ ,
 /*E409*/ GENx___x___x___ ,
 /*E40A*/ GENx___x___x___ ,
 /*E40B*/ GENx___x___x___ ,
 /*E40C*/ GENx___x___x___ ,
 /*E40D*/ GENx___x___x___ ,
 /*E40E*/ GENx___x___x___ ,
 /*E40F*/ GENx___x___x___ ,
 /*E410*/ GENx___x___x___ ,
 /*E411*/ GENx___x___x___ ,
 /*E412*/ GENx___x___x___ ,
 /*E413*/ GENx___x___x___ ,
 /*E414*/ GENx___x___x___ ,
 /*E415*/ GENx___x___x___ ,
 /*E416*/ GENx___x___x___ ,
 /*E417*/ GENx___x___x___ ,
 /*E418*/ GENx___x___x___ ,
 /*E419*/ GENx___x___x___ ,
 /*E41A*/ GENx___x___x___ ,
 /*E41B*/ GENx___x___x___ ,
 /*E41C*/ GENx___x___x___ ,
 /*E41D*/ GENx___x___x___ ,
 /*E41E*/ GENx___x___x___ ,
 /*E41F*/ GENx___x___x___ ,
 /*E420*/ GENx___x___x___ ,
 /*E421*/ GENx___x___x___ ,
 /*E422*/ GENx___x___x___ ,
 /*E423*/ GENx___x___x___ ,
 /*E424*/ GENx___x___x___ ,
 /*E425*/ GENx___x___x___ ,
 /*E426*/ GENx___x___x___ ,
 /*E427*/ GENx___x___x___ ,
 /*E428*/ GENx___x___x___ ,
 /*E429*/ GENx___x___x___ ,
 /*E42A*/ GENx___x___x___ ,
 /*E42B*/ GENx___x___x___ ,
 /*E42C*/ GENx___x___x___ ,
 /*E42D*/ GENx___x___x___ ,
 /*E42E*/ GENx___x___x___ ,
 /*E42F*/ GENx___x___x___ ,
 /*E430*/ GENx___x___x___ ,
 /*E431*/ GENx___x___x___ ,
 /*E432*/ GENx___x___x___ ,
 /*E433*/ GENx___x___x___ ,
 /*E434*/ GENx___x___x___ ,
 /*E435*/ GENx___x___x___ ,
 /*E436*/ GENx___x___x___ ,
 /*E437*/ GENx___x___x___ ,
 /*E438*/ GENx___x___x___ ,
 /*E439*/ GENx___x___x___ ,
 /*E43A*/ GENx___x___x___ ,
 /*E43B*/ GENx___x___x___ ,
 /*E43C*/ GENx___x___x___ ,
 /*E43D*/ GENx___x___x___ ,
 /*E43E*/ GENx___x___x___ ,
 /*E43F*/ GENx___x___x___ ,
 /*E440*/ GENx___x___x___ ,
 /*E441*/ GENx___x___x___ ,
 /*E442*/ GENx___x___x___ ,
 /*E443*/ GENx___x___x___ ,
 /*E444*/ GENx___x___x___ ,
 /*E445*/ GENx___x___x___ ,
 /*E446*/ GENx___x___x___ ,
 /*E447*/ GENx___x___x___ ,
 /*E448*/ GENx___x___x___ ,
 /*E449*/ GENx___x___x___ ,
 /*E44A*/ GENx___x___x___ ,
 /*E44B*/ GENx___x___x___ ,
 /*E44C*/ GENx___x___x___ ,
 /*E44D*/ GENx___x___x___ ,
 /*E44E*/ GENx___x___x___ ,
 /*E44F*/ GENx___x___x___ ,
 /*E450*/ GENx___x___x___ ,
 /*E451*/ GENx___x___x___ ,
 /*E452*/ GENx___x___x___ ,
 /*E453*/ GENx___x___x___ ,
 /*E454*/ GENx___x___x___ ,
 /*E455*/ GENx___x___x___ ,
 /*E456*/ GENx___x___x___ ,
 /*E457*/ GENx___x___x___ ,
 /*E458*/ GENx___x___x___ ,
 /*E459*/ GENx___x___x___ ,
 /*E45A*/ GENx___x___x___ ,
 /*E45B*/ GENx___x___x___ ,
 /*E45C*/ GENx___x___x___ ,
 /*E45D*/ GENx___x___x___ ,
 /*E45E*/ GENx___x___x___ ,
 /*E45F*/ GENx___x___x___ ,
 /*E460*/ GENx___x___x___ ,
 /*E461*/ GENx___x___x___ ,
 /*E462*/ GENx___x___x___ ,
 /*E463*/ GENx___x___x___ ,
 /*E464*/ GENx___x___x___ ,
 /*E465*/ GENx___x___x___ ,
 /*E466*/ GENx___x___x___ ,
 /*E467*/ GENx___x___x___ ,
 /*E468*/ GENx___x___x___ ,
 /*E469*/ GENx___x___x___ ,
 /*E46A*/ GENx___x___x___ ,
 /*E46B*/ GENx___x___x___ ,
 /*E46C*/ GENx___x___x___ ,
 /*E46D*/ GENx___x___x___ ,
 /*E46E*/ GENx___x___x___ ,
 /*E46F*/ GENx___x___x___ ,
 /*E470*/ GENx___x___x___ ,
 /*E471*/ GENx___x___x___ ,
 /*E472*/ GENx___x___x___ ,
 /*E473*/ GENx___x___x___ ,
 /*E474*/ GENx___x___x___ ,
 /*E475*/ GENx___x___x___ ,
 /*E476*/ GENx___x___x___ ,
 /*E477*/ GENx___x___x___ ,
 /*E478*/ GENx___x___x___ ,
 /*E479*/ GENx___x___x___ ,
 /*E47A*/ GENx___x___x___ ,
 /*E47B*/ GENx___x___x___ ,
 /*E47C*/ GENx___x___x___ ,
 /*E47D*/ GENx___x___x___ ,
 /*E47E*/ GENx___x___x___ ,
 /*E47F*/ GENx___x___x___ ,
 /*E480*/ GENx___x___x___ ,
 /*E481*/ GENx___x___x___ ,
 /*E482*/ GENx___x___x___ ,
 /*E483*/ GENx___x___x___ ,
 /*E484*/ GENx___x___x___ ,
 /*E485*/ GENx___x___x___ ,
 /*E486*/ GENx___x___x___ ,
 /*E487*/ GENx___x___x___ ,
 /*E488*/ GENx___x___x___ ,
 /*E489*/ GENx___x___x___ ,
 /*E48A*/ GENx___x___x___ ,
 /*E48B*/ GENx___x___x___ ,
 /*E48C*/ GENx___x___x___ ,
 /*E48D*/ GENx___x___x___ ,
 /*E48E*/ GENx___x___x___ ,
 /*E48F*/ GENx___x___x___ ,
 /*E490*/ GENx___x___x___ ,
 /*E491*/ GENx___x___x___ ,
 /*E492*/ GENx___x___x___ ,
 /*E493*/ GENx___x___x___ ,
 /*E494*/ GENx___x___x___ ,
 /*E495*/ GENx___x___x___ ,
 /*E496*/ GENx___x___x___ ,
 /*E497*/ GENx___x___x___ ,
 /*E498*/ GENx___x___x___ ,
 /*E499*/ GENx___x___x___ ,
 /*E49A*/ GENx___x___x___ ,
 /*E49B*/ GENx___x___x___ ,
 /*E49C*/ GENx___x___x___ ,
 /*E49D*/ GENx___x___x___ ,
 /*E49E*/ GENx___x___x___ ,
 /*E49F*/ GENx___x___x___ ,
 /*E4A0*/ GENx___x___x___ ,
 /*E4A1*/ GENx___x___x___ ,
 /*E4A2*/ GENx___x___x___ ,
 /*E4A3*/ GENx___x___x___ ,
 /*E4A4*/ GENx___x___x___ ,
 /*E4A5*/ GENx___x___x___ ,
 /*E4A6*/ GENx___x___x___ ,
 /*E4A7*/ GENx___x___x___ ,
 /*E4A8*/ GENx___x___x___ ,
 /*E4A9*/ GENx___x___x___ ,
 /*E4AA*/ GENx___x___x___ ,
 /*E4AB*/ GENx___x___x___ ,
 /*E4AC*/ GENx___x___x___ ,
 /*E4AD*/ GENx___x___x___ ,
 /*E4AE*/ GENx___x___x___ ,
 /*E4AF*/ GENx___x___x___ ,
 /*E4B0*/ GENx___x___x___ ,
 /*E4B1*/ GENx___x___x___ ,
 /*E4B2*/ GENx___x___x___ ,
 /*E4B3*/ GENx___x___x___ ,
 /*E4B4*/ GENx___x___x___ ,
 /*E4B5*/ GENx___x___x___ ,
 /*E4B6*/ GENx___x___x___ ,
 /*E4B7*/ GENx___x___x___ ,
 /*E4B8*/ GENx___x___x___ ,
 /*E4B9*/ GENx___x___x___ ,
 /*E4BA*/ GENx___x___x___ ,
 /*E4BB*/ GENx___x___x___ ,
 /*E4BC*/ GENx___x___x___ ,
 /*E4BD*/ GENx___x___x___ ,
 /*E4BE*/ GENx___x___x___ ,
 /*E4BF*/ GENx___x___x___ ,
 /*E4C0*/ GENx___x___x___ ,
 /*E4C1*/ GENx___x___x___ ,
 /*E4C2*/ GENx___x___x___ ,
 /*E4C3*/ GENx___x___x___ ,
 /*E4C4*/ GENx___x___x___ ,
 /*E4C5*/ GENx___x___x___ ,
 /*E4C6*/ GENx___x___x___ ,
 /*E4C7*/ GENx___x___x___ ,
 /*E4C8*/ GENx___x___x___ ,
 /*E4C9*/ GENx___x___x___ ,
 /*E4CA*/ GENx___x___x___ ,
 /*E4CB*/ GENx___x___x___ ,
 /*E4CC*/ GENx___x___x___ ,
 /*E4CD*/ GENx___x___x___ ,
 /*E4CE*/ GENx___x___x___ ,
 /*E4CF*/ GENx___x___x___ ,
 /*E4D0*/ GENx___x___x___ ,
 /*E4D1*/ GENx___x___x___ ,
 /*E4D2*/ GENx___x___x___ ,
 /*E4D3*/ GENx___x___x___ ,
 /*E4D4*/ GENx___x___x___ ,
 /*E4D5*/ GENx___x___x___ ,
 /*E4D6*/ GENx___x___x___ ,
 /*E4D7*/ GENx___x___x___ ,
 /*E4D8*/ GENx___x___x___ ,
 /*E4D9*/ GENx___x___x___ ,
 /*E4DA*/ GENx___x___x___ ,
 /*E4DB*/ GENx___x___x___ ,
 /*E4DC*/ GENx___x___x___ ,
 /*E4DD*/ GENx___x___x___ ,
 /*E4DE*/ GENx___x___x___ ,
 /*E4DF*/ GENx___x___x___ ,
 /*E4E0*/ GENx___x___x___ ,
 /*E4E1*/ GENx___x___x___ ,
 /*E4E2*/ GENx___x___x___ ,
 /*E4E3*/ GENx___x___x___ ,
 /*E4E4*/ GENx___x___x___ ,
 /*E4E5*/ GENx___x___x___ ,
 /*E4E6*/ GENx___x___x___ ,
 /*E4E7*/ GENx___x___x___ ,
 /*E4E8*/ GENx___x___x___ ,
 /*E4E9*/ GENx___x___x___ ,
 /*E4EA*/ GENx___x___x___ ,
 /*E4EB*/ GENx___x___x___ ,
 /*E4EC*/ GENx___x___x___ ,
 /*E4ED*/ GENx___x___x___ ,
 /*E4EE*/ GENx___x___x___ ,
 /*E4EF*/ GENx___x___x___ ,
 /*E4F0*/ GENx___x___x___ ,
 /*E4F1*/ GENx___x___x___ ,
 /*E4F2*/ GENx___x___x___ ,
 /*E4F3*/ GENx___x___x___ ,
 /*E4F4*/ GENx___x___x___ ,
 /*E4F5*/ GENx___x___x___ ,
 /*E4F6*/ GENx___x___x___ ,
 /*E4F7*/ GENx___x___x___ ,
 /*E4F8*/ GENx___x___x___ ,
 /*E4F9*/ GENx___x___x___ ,
 /*E4FA*/ GENx___x___x___ ,
 /*E4FB*/ GENx___x___x___ ,
 /*E4FC*/ GENx___x___x___ ,
 /*E4FD*/ GENx___x___x___ ,
 /*E4FE*/ GENx___x___x___ ,
 /*E4FF*/ GENx___x___x___  };

// #endif /*defined (FEATURE_VECTOR_FACILITY)*/

#endif /*!defined (_GEN_ARCH)*/

/* end of OPCODE.C */
