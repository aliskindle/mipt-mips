/**
 * data_bypass.h - Implementation of bypassing unit
 * 
 * @author Denis Los
 * Copyright 2018 MIPT-MIPS Project
 */

#ifndef DATA_BYPASS_H
#define DATA_BYPASS_H

#include <array>

#include <modules/core/perf_instr.h>

#include "data_bypass_interface.h"

template <typename ISA>
class DataBypass
{
    using FuncInstr = typename ISA::FuncInstr;
    using Register  = typename ISA::Register;
    using Instr     = PerfInstr<FuncInstr>;
    using RegDstUInt = typename ISA::RegDstUInt;

    public:
        explicit DataBypass( uint64 complex_alu_latency)
            : last_execution_stage_value( static_cast<uint8>(complex_alu_latency - 1))
        { }

        // checks whether the source register of passed instruction is in RF  
        auto is_in_RF( const Instr& instr, uint8 src_index) const
        {
            const auto reg_num = instr.get_src_num( src_index);
            return get_entry( reg_num).current_stage.is_in_RF();
        }

        // checks whether the source register of passed instruction is bypassible
        auto is_bypassible( const Instr& instr, uint8 src_index) const
        {
            const auto reg_num = instr.get_src_num( src_index);
            return get_entry( reg_num).is_bypassible;
        }

        // checks if the stall needed for passed instruction
        auto is_stall( const Instr& instr) const
        {
            const auto instruction_latency = get_instruction_latency( instr);
    
            return (( !is_in_RF( instr, 0) && !is_bypassible( instr, 0)) ||
                    ( !is_in_RF( instr, 1) && !is_bypassible( instr, 1)) ||
                    ( instruction_latency < writeback_stage_info.operation_latency));
        }

        // returns bypass command for passed instruction and its source register
        // in accordance with current state of the scoreboard
        auto get_bypass_command( const Instr& instr, uint8 src_index) const
        {
            const auto reg_num = instr.get_src_num( src_index);
            return BypassCommand<Register>( get_current_stage( reg_num), reg_num, last_execution_stage_value);
        }

        // introduces new instruction to bypassing unit
        void trace_new_instr( const Instr& instr);

        // updates the scoreboard
        void update();

        // handle pipeline flush
        void handle_flush();
    
    private:
        const uint8 last_execution_stage_value;

        struct RegisterInfo
        {
            RegisterStage current_stage;
            RegisterStage ready_stage;
            RegisterStage next_stage_after_first_execution_stage;
            bool is_bypassible = false;
            bool is_traced = false;

            void reset() { *this = RegisterInfo(); }
        };

        struct FuncUnitInfo
        {
            Latency operation_latency = 0_Lt;

            void update()
            {
                if ( operation_latency != 0_Lt)
                    operation_latency = operation_latency - 1_Lt;
            }
        };

        std::array<RegisterInfo, Register::MAX_REG> scoreboard = {};
        FuncUnitInfo writeback_stage_info = {};

        RegisterInfo& get_entry( Register num)
        {
            return scoreboard.at( num.to_size_t());
        }

        const RegisterInfo& get_entry( Register num) const
        {
            return scoreboard.at( num.to_size_t());
        }

        // returns current stage of passed register
        // in accordance with the current state of the scoreboard
        RegisterStage get_current_stage( Register num) const
        {
            return get_entry( num).current_stage;
        }

        // returns a latency of passed instruction
        // in accordance with a type of the instruction
        Latency get_instruction_latency( const Instr& instr) const
        {
            if ( instr.is_mem_stage_required())
                return 2_Lt;
            
            if ( instr.is_complex_arithmetic())
                return Latency( last_execution_stage_value + 1);
            
            return 1_Lt;
        }

        // introduces a source register of passed instruction to the scoreboard 
        void trace_new_register( const Instr& instr, Register num);
};

template <typename ISA>
void DataBypass<ISA>::trace_new_register( const Instr& instr, Register num)
{
    auto& entry = get_entry( num);

    entry.current_stage.set_to_first_execution_stage();
    entry.ready_stage.set_to_first_execution_stage();


    if ( instr.is_complex_arithmetic())
    {
        entry.ready_stage.set_to_last_execution_stage( last_execution_stage_value);

        entry.next_stage_after_first_execution_stage.set_to_first_execution_stage();
        entry.next_stage_after_first_execution_stage.inc();
    }
    else if ( instr.is_mem_stage_required())
    {
        entry.next_stage_after_first_execution_stage.set_to_mem_stage();

        if ( instr.is_load())
            entry.ready_stage.set_to_mem_stage();
    }
    else
        entry.next_stage_after_first_execution_stage.set_to_writeback();


    if ( !instr.is_bypassible())
        entry.ready_stage.set_to_in_RF();


    entry.is_bypassible = ( entry.current_stage == entry.ready_stage);
    entry.is_traced = true;
}


template <typename ISA>
void DataBypass<ISA>::trace_new_instr( const Instr& instr)
{    
    const auto dst_reg_num = instr.get_dst_num();

    writeback_stage_info.operation_latency = get_instruction_latency( instr);

    if ( dst_reg_num.is_zero())
        return;
    
    if ( dst_reg_num.is_mips_hi_lo())
    {
        trace_new_register( instr, Register::mips_lo );
        trace_new_register( instr, Register::mips_hi );
        return;
    }

    trace_new_register( instr, dst_reg_num);
}


template <typename ISA>
void DataBypass<ISA>::update()
{
    for ( auto& entry:scoreboard)
    {
        if ( entry.is_traced)
        {
            if ( entry.current_stage.is_writeback())
            {
                entry.reset();
            }
            else
            {
                if ( entry.current_stage.is_first_execution_stage())
                {
                    entry.current_stage = entry.next_stage_after_first_execution_stage;
                }
                else if ( entry.current_stage.is_last_execution_stage( last_execution_stage_value))
                {
                    entry.current_stage.set_to_writeback();
                }
                else
                    entry.current_stage.inc();
                

                if ( entry.current_stage == entry.ready_stage)
                    entry.is_bypassible = true;   
            }
        }
    }

    writeback_stage_info.update();
}


template <typename ISA>
void DataBypass<ISA>::handle_flush()
{
    for ( auto& entry:scoreboard)
    {
        if ( entry.is_traced)
            entry.reset();
    }

    writeback_stage_info.operation_latency = 0_Lt;
}

#endif // DATA_BYPASS_H
