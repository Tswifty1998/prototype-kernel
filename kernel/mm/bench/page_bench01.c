/*
 * Benchmarking page allocator execution time inside the kernel
 *
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/time.h>
#include <linux/time_bench.h>
#include <linux/spinlock.h>
#include <linux/mm.h>

static int verbose=1;

static int time_single_page_alloc_put(
	struct time_bench_record *rec, void *data)
{
	gfp_t gfp_mask = (GFP_ATOMIC | ___GFP_NORETRY);
	struct page *my_page;
	int i;

	time_bench_start(rec);
	/** Loop to measure **/
	for (i = 0; i < rec->loops; i++) {
		my_page = alloc_page(gfp_mask);
		if (unlikely(my_page == NULL))
			return 0;
		__free_page(my_page);
	}
	time_bench_stop(rec, i);
	return i;
}

static int time_alloc_pages(
	struct time_bench_record *rec, void *data)
{
	/* Important to set: __GFP_COMP for compound pages
	 */
	gfp_t gfp_mask = (GFP_ATOMIC | __GFP_COLD | __GFP_COMP);
	struct page *my_page;
	int order = rec->step;
	int i;

	/* Drop WARN on failures, time_bench will invalidate test */
	gfp_mask |= __GFP_NOWARN;

	time_bench_start(rec);
	/** Loop to measure **/
	for (i = 0; i < rec->loops; i++) {
		my_page = alloc_pages(gfp_mask, order);
		if (unlikely(my_page == NULL))
			return 0;
		__free_pages(my_page, order);
	}
	time_bench_stop(rec, i);

	if (verbose) {
		time_bench_calc_stats(rec);
		pr_info("alloc_pages order:%d(%luB/x%d) %llu cycles"
			" per-%luB %llu cycles\n",
			order, PAGE_SIZE << order, 1 << order,
			rec->tsc_cycles, PAGE_SIZE,
			rec->tsc_cycles >> order);
	}

	return i;
}

static int time_alloc_pages_with_fallback(
	struct time_bench_record *rec, void *data)
{
	gfp_t gfp_mask = (GFP_ATOMIC | __GFP_COLD);
	struct page *page;
	int preferred_order = rec->step;
	int i, order;
	int histogram_order[MAX_ORDER] = {0};

	time_bench_start(rec);
	/** Loop to measure **/
	for (i = 0; i < rec->loops; i++) {

		/* Simulate system in mlx4_alloc_pages() */
		for (order = preferred_order; ;) {
			gfp_t gfp = gfp_mask;

			if (order)
				gfp |= __GFP_COMP | __GFP_NOWARN;
			page = alloc_pages(gfp, order);
			if (likely(page)) {
				histogram_order[order]++;
				break;
			}
			if (--order < 0)
				return 0; // -ENOMEM;
		}
		if (unlikely(page == NULL))
			return 0;
		__free_pages(page, order);
	}
	time_bench_stop(rec, i);

	/* Display which order sizes that got used */
	if (verbose) {
		int j;

		pr_info("Histgram order(max:%d): ", preferred_order);
		for (j = 0; j <= preferred_order; j++) {
			printk("[%d]=%d ", j, histogram_order[j]);
		}
		printk("\n");
	}

	return i;
}

int run_timing_tests(void)
{
	uint32_t loops = 100000;

	time_bench_loop(loops, 0, "single_page_alloc_put",
			NULL, time_single_page_alloc_put);

	time_bench_loop(loops, 0, "alloc_pages_order0", NULL, time_alloc_pages);
	time_bench_loop(loops, 1, "alloc_pages_order1", NULL, time_alloc_pages);
	time_bench_loop(loops, 2, "alloc_pages_order2", NULL, time_alloc_pages);
	time_bench_loop(loops, 3, "alloc_pages_order3", NULL, time_alloc_pages);
	time_bench_loop(loops, 4, "alloc_pages_order4", NULL, time_alloc_pages);
	time_bench_loop(loops, 5, "alloc_pages_order5", NULL, time_alloc_pages);
	time_bench_loop(loops, 6, "alloc_pages_order6", NULL, time_alloc_pages);
	time_bench_loop(loops, 7, "alloc_pages_order7", NULL, time_alloc_pages);
	time_bench_loop(loops, 8, "alloc_pages_order8", NULL, time_alloc_pages);
	time_bench_loop(loops, 9, "alloc_pages_order9", NULL, time_alloc_pages);

	time_bench_loop(loops, 5, "alloc_pages_with_fallback",
			NULL, time_alloc_pages_with_fallback);

	return 0;
}

static int __init page_bench01_module_init(void)
{
	if (verbose)
		pr_info("Loaded\n");

#ifdef CONFIG_DEBUG_PREEMPT
	pr_warn("WARN: CONFIG_DEBUG_PREEMPT is enabled: this affect results\n");
#endif
	if (run_timing_tests() < 0) {
		return -ECANCELED;
	}

	return 0;
}
module_init(page_bench01_module_init);

static void __exit page_bench01_module_exit(void)
{
	if (verbose)
		pr_info("Unloaded\n");
}
module_exit(page_bench01_module_exit);

MODULE_DESCRIPTION("Benchmarking page alloactor execution time in kernel");
MODULE_AUTHOR("Jesper Dangaard Brouer <netoptimizer@brouer.com>");
MODULE_LICENSE("GPL");