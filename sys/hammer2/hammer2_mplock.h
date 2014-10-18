static __inline void
__mp_lock_spin(struct __mp_lock *mpl, u_int me)
{
#ifndef MP_LOCKDEBUG
        while (mpl->mpl_ticket != me)
                SPINLOCK_SPIN_HOOK;
#else
        int ticks = __mp_lock_spinout;

        while (mpl->mpl_ticket != me && --ticks > 0)
                SPINLOCK_SPIN_HOOK;

        if (ticks == 0) {
                db_printf("__mp_lock(%p): lock spun out", mpl);
                Debugger();
        }
#endif
}

static inline u_int
fetch_and_add(u_int *var, u_int value)
{
        asm volatile("lock; xaddl %%eax, %2;"
            : "=a" (value)
            : "a" (value), "m" (*var)
            : "memory");

        return (value);
}

void
__mp_lock(struct __mp_lock *mpl)
{
        struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
        long rf = read_rflags();

        disable_intr();
        if (cpu->mplc_depth++ == 0)
                cpu->mplc_ticket = fetch_and_add(&mpl->mpl_users, 1);
        write_rflags(rf);

        __mp_lock_spin(mpl, cpu->mplc_ticket);
}

void
__mp_unlock(struct __mp_lock *mpl)
{
        struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
        long rf = read_rflags();

#ifdef MP_LOCKDEBUG
        if (!__mp_lock_held(mpl)) {
                db_printf("__mp_unlock(%p): not held lock\n", mpl);
                Debugger();
        }
#endif

        disable_intr();
        if (--cpu->mplc_depth == 0)
                mpl->mpl_ticket++;
        write_rflags(rf);
}
