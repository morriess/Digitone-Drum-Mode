| SPDX-License-Identifier: GPL-2.0-or-later
| Drum Mode 0.11: the OS 1.44 task and semaphore routines its note task uses, as DigiArp found them (its fast.s).
| Drum Mode's own names, so a build with DigiArp has no two of the same.
.text
.globl dm_os_task_create, dm_os_task_start, dm_os_sem_init, dm_os_sem_pend, dm_os_sem_post
dm_os_task_create: jmp 0x4000170c       | (tcb, fn, priority, stack, size)
dm_os_task_start:  jmp 0x40001758       | (tcb)
dm_os_sem_init:    jmp 0x400017dc       | (sem, count)
dm_os_sem_pend:    jmp 0x400017ea       | (sem)
dm_os_sem_post:    jmp 0x400018d0       | (sem)
