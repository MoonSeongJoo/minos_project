/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/irq.h>
#include <minos/sched.h>
#include <virt/vm.h>

static inline void irq_handler(gp_regs *regs)
{
	do_irq_handler();
}

void irq_from_lower_el(gp_regs *regs)
{
	irq_handler(regs);
}

// void irq_from_lower_el(gp_regs *regs)
// {
//     static int irq_lower_trace_count;

//     if (current && task_is_vcpu(current) && irq_lower_trace_count++ < 10) {
//         struct vcpu *vcpu = get_current_vcpu();

//         pr_notice("GUEST_IRQ_TRACE: vm%d vcpu%d pc=0x%lx pstate=0x%lx\n",
//                   vcpu->vm->vmid,
//                   get_vcpu_id(vcpu),
//                   regs->pc,
//                   regs->pstate);
//     }

//     irq_handler(regs);
// }

void irq_from_current_el(gp_regs *regs)
{
	irq_handler(regs);
}
