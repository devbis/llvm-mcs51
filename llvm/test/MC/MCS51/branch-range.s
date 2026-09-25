# RUN: not llvm-mc -triple=mcs51 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

        .text
        sjmp    too_far
        .space  128
too_far:
        nop

# CHECK: MCS-51 relative branch is out of range
