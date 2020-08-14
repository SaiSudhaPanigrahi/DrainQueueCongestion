#include "ns-tcp.h"
int sysctl_tcp_min_rtt_wlen = 300;
void tcp_update_rtt_min(struct sock *sk, u32 rtt_us)
{
	struct tcp_sock *tp = tcp_sk(sk);
	u32 wlen = sysctl_tcp_min_rtt_wlen * HZ;

	minmax_running_min(&tp->rtt_min, wlen, tcp_time_stamp,
			   rtt_us /*? : jiffies_to_usecs(1)*/);
}


int fls(int x)
{
        int r = 32;

        if (!x)
                return 0;
        if (!(x & 0xffff0000u)) {
                x <<= 16;
                r -= 16;
        }
        if (!(x & 0xff000000u)) {
                x <<= 8;
                r -= 8;
        }
        if (!(x & 0xf0000000u)) {
                x <<= 4;
                r -= 4;
        }
        if (!(x & 0xc0000000u)) {
                x <<= 2;
                r -= 2;
        }
        if (!(x & 0x80000000u)) {
                x <<= 1;
                r -= 1;
        }
        return r;
}

int fls64(__u64 x)
{
        __u32 h = x >> 32;
        if (h)
                return fls(h) + 32;
        return fls(x);
}



/* 64bit divisor, dividend and result. dynamic precision */
uint64_t div64_64(uint64_t dividend, uint64_t divisor)
{
	uint32_t high, d;

	high = divisor >> 32;
	if (high) {
		unsigned int shift = fls(high);

		d = divisor >> shift;
		dividend >>= shift;
	} else
		d = divisor;

	do_div(dividend, d);

	return dividend;
}
u64 div64_u64(u64 dividend, u64 divisor)
{
	u32 high = divisor >> 32;
	u64 quot;

	if (high == 0) {
		quot = div_u64(dividend, divisor);
	} else {
		int n = fls(high);
		quot = div_u64(dividend >> n, divisor >> n);

		if (quot != 0)
			quot--;
		if ((dividend - quot * divisor) >= divisor)
			quot++;
	}
	return quot;
}
s64 div64_s64(s64 dividend, s64 divisor)
{
	s64 quot, t;

	quot = div64_u64(abs(dividend), abs(divisor));
	t = (dividend ^ divisor) >> 63;

	return (quot ^ t) - t;
}
