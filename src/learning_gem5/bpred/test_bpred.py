import argparse
import os
import sys

import m5
from m5.objects.BaseMinorCPU import *
from m5.objects.RiscvCPU import RiscvMinorCPU

from gem5.components.boards.simple_board import SimpleBoard
from gem5.components.cachehierarchies.classic.private_l1_cache_hierarchy import (
    PrivateL1CacheHierarchy,
)
from gem5.components.memory.single_channel import SingleChannelDDR3_1600
from gem5.components.processors.base_cpu_core import BaseCPUCore
from gem5.components.processors.base_cpu_processor import BaseCPUProcessor
from gem5.isas import ISA
from gem5.resources.resource import CustomResource
from gem5.simulate.simulator import Simulator


class YAGS(YagsBP):
    globalPredictorSize = 8192


class BranchTestCPU(RiscvMinorCPU):
    """
    The fetch, decode, and execute stage parameters from the ARM HPI CPU
    This information about the CPU can be found on page 15 of
    `gem5_rsk_gem5-21.2.pdf` at https://github.com/arm-university/arm-gem5-rsk

    The parameters that are changed are:
    - threadPolicy:
        This is initialized to "SingleThreaded".
    - decodeToExecuteForwardDelay:
        This is changed from 1 to 2 to avoid a PMC address fault.
    - fetch1ToFetch2BackwardDelay:
        This is changed from 1 to 0 to better match hardware performance.
    - fetch2InputBufferSize:
        This is changed from 2 to 1 to better match hardware performance.
    - decodeInputBufferSize:
        This is changed from 3 to 2 to better match hardware performance.
    - decodeToExecuteForwardDelay:
        This is changed from 2 to 1 to better match hardware performance.
    - executeInputBufferSize:
        This is changed from 7 to 4 to better match hardware performance.
    - executeMaxAccessesInMemory:
        This is changed from 2 to 1 to better match hardware performance.
    - executeLSQStoreBufferSize:
        This is changed from 5 to 3 to better match hardware performance.
    - executeBranchDelay:
        This is changed from 1 to 2 to better match hardware performance.
    - enableIdling:
        This is changed to False to better match hardware performance.

    """

    threadPolicy = "SingleThreaded"

    # Fetch1 stage
    fetch1LineSnapWidth = 0
    fetch1LineWidth = 0
    fetch1FetchLimit = 1
    fetch1ToFetch2ForwardDelay = 1
    fetch1ToFetch2BackwardDelay = 0

    # Fetch2 stage
    fetch2InputBufferSize = 1
    fetch2ToDecodeForwardDelay = 1
    fetch2CycleInput = True

    # Decode stage
    decodeInputBufferSize = 2
    decodeToExecuteForwardDelay = 1
    decodeInputWidth = 2
    decodeCycleInput = True

    # Execute stage
    executeInputWidth = 2
    executeCycleInput = True
    executeIssueLimit = 2
    executeMemoryIssueLimit = 1
    executeCommitLimit = 2
    executeMemoryCommitLimit = 1
    executeInputBufferSize = 4
    executeMaxAccessesInMemory = 1
    executeLSQMaxStoreBufferStoresPerCycle = 2
    executeLSQRequestsQueueSize = 1
    executeLSQTransfersQueueSize = 2
    executeLSQStoreBufferSize = 3
    executeBranchDelay = 2
    executeSetTraceTimeOnCommit = True
    executeSetTraceTimeOnIssue = False
    executeAllowEarlyMemoryIssue = True
    enableIdling = False

    # Functional Units and Branch Prediction
    branchPred = YAGS()


class BranchTestCPUCore(BaseCPUCore):
    def __init__(
        self,
        core_id,
    ):
        super().__init__(core=BranchTestCPU(cpu_id=core_id), isa=ISA.RISCV)
        self.core.isa[0].enable_rvv = False


parser = argparse.ArgumentParser()
parser.add_argument(
    "-c",
    "--cmd",
    default="",
    required=True,
    help="The binary to run in syscall emulation mode.",
)
args = parser.parse_args()


def main():
    exe_file = args.cmd
    if os.path.exists(exe_file):
        binary = CustomResource(exe_file)
    else:
        print(f"Error, {file} not exist")
        sys.exit(-1)

    processor = BaseCPUProcessor(
        cores=[BranchTestCPUCore(0)],
    )

    cache_hierachy = PrivateL1CacheHierarchy(l1d_size="1MiB", l1i_size="1MiB")
    memory = SingleChannelDDR3_1600("1GiB")
    memory.set_memory_range(
        [AddrRange(start=0x80000000, size=memory.get_size())]
    )
    board = SimpleBoard(
        clk_freq="3GHz",
        processor=processor,
        memory=memory,
        cache_hierarchy=cache_hierachy,
    )

    board.set_se_binary_workload(binary)

    simulator = Simulator(board=board)
    simulator.run()


if __name__ != "__main__":
    main()
