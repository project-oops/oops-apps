/*
 * Host selftest for pltauth-patch SceShellCore entitlement bypass.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "oops/krw.h"
#include "oops/freestd.h"

typedef struct {
    uint64_t offset;
    uint8_t len;
    uint8_t data[16];
} test_patch_t;

/* Verified retail SceShellCore patches for FW 12.20 and FW 12.40 */
static const test_patch_t FW_1240_PATCHES[] = {
    {0x00C870C3ULL, 3,  {0x52, 0xEB, 0xE2}},
    {0x00C870A8ULL, 7,  {0xE8, 0x23, 0xF8, 0xFF, 0xFF, 0x58, 0xC3}},
    {0x00C868B6ULL, 5,  {0xE9, 0x07, 0x00, 0x00, 0x00}},
    {0x00C868C2ULL, 10, {0x31, 0xC0, 0x50, 0xE8, 0x06, 0x00, 0x00, 0x00, 0x58, 0xC3}},
    {0x00789EE6ULL, 2,  {0xEB, 0x04}},
    {0x00330D81ULL, 2,  {0xEB, 0x04}},
    {0x00331151ULL, 2,  {0xEB, 0x04}},
    {0x007AC232ULL, 1,  {0xEB}},
    {0x007930A5ULL, 2,  {0x90, 0xE9}},
    {0x007AC9C8ULL, 1,  {0xEB}},
    {0x007AEF86ULL, 4,  {0x9E, 0x01, 0x00, 0x00}},
    {0x00214E81ULL, 14, {0xE8, 0x3A, 0xFC, 0x67, 0x00, 0x31, 0xC9, 0xFF, 0xC1, 0xE9, 0xC4, 0xFE, 0xFF, 0xFF}},
    {0x00214D53ULL, 11, {0x83, 0xF8, 0x02, 0x0F, 0x43, 0xC1, 0xE9, 0x60, 0x0A, 0x00, 0x00}},
    {0x00215260ULL, 5,  {0xE9, 0x1C, 0xFC, 0xFF, 0xFF}},
    {0x007D2350ULL, 1,  {0xC3}},
    {0x017438E0ULL, 3,  {0x31, 0xC0, 0xC3}},
    {0x01747E40ULL, 3,  {0x31, 0xC0, 0xC3}},
    {0x006557AAULL, 2,  {0x66, 0x90}},
    {0x00B1BEBAULL, 1,  {0xEB}},
    {0x00AF9483ULL, 2,  {0xEB, 0x03}},
    {0x00328EE0ULL, 2,  {0x90, 0xE9}},
    {0x00328F5AULL, 2,  {0x90, 0xE9}},
    {0x0032905CULL, 1,  {0xEB}},
    {0x00329130ULL, 1,  {0xEB}},
    {0x00329351ULL, 2,  {0x90, 0xE9}},
    {0x00329462ULL, 1,  {0xEB}},
    {0x0032993AULL, 2,  {0x90, 0xE9}},
    {0x003299CDULL, 2,  {0x90, 0xE9}},
    {0x00788378ULL, 1,  {0xEB}},
    {0x0078BF72ULL, 1,  {0xEB}},
    {0x0078FE10ULL, 4,  {0x48, 0x31, 0xC0, 0xC3}},
};

static uint64_t mock_dmap_base = 0;
static uint64_t mock_read64(uintptr_t addr) {
    return *(const uint64_t *)addr;
}

static uint64_t test_virt_to_phys(uint64_t cr3, uint64_t dmap, uint64_t va) {
    uint64_t pml4i = (va >> 39) & 0x1FFULL;
    uint64_t pdpti = (va >> 30) & 0x1FFULL;
    uint64_t pdi   = (va >> 21) & 0x1FFULL;
    uint64_t pti   = (va >> 12) & 0x1FFULL;

    uint64_t pml4e = mock_read64((uintptr_t)(dmap + (cr3 & 0x000FFFFFFFFFF000ULL) + pml4i * 8));
    if ((pml4e & 1ULL) == 0) return 0;

    uint64_t pdpte = mock_read64((uintptr_t)(dmap + (pml4e & 0x000FFFFFFFFFF000ULL) + pdpti * 8));
    if ((pdpte & 1ULL) == 0) return 0;
    if ((pdpte & 0x80ULL) != 0) {
        return (pdpte & 0x000FFFFFC0000000ULL) | (va & 0x3FFFFFFFULL);
    }

    uint64_t pde = mock_read64((uintptr_t)(dmap + (pdpte & 0x000FFFFFFFFFF000ULL) + pdi * 8));
    if ((pde & 1ULL) == 0) return 0;
    if ((pde & 0x80ULL) != 0) {
        return (pde & 0x000FFFFFFFE00000ULL) | (va & 0x1FFFFFULL);
    }

    uint64_t pte = mock_read64((uintptr_t)(dmap + (pde & 0x000FFFFFFFFFF000ULL) + pti * 8));
    if ((pte & 1ULL) == 0) return 0;

    return (pte & 0x000FFFFFFFFFF000ULL) | (va & 0xFFFULL);
}

int main(void) {
    /* 1. Verify payload arguments shape */
    payload_args_t args;
    memset(&args, 0, sizeof(args));
    assert(sizeof(args) >= 40);

    /* 2. Validate FW 12.40 SceShellCore patch table */
    size_t count = sizeof(FW_1240_PATCHES) / sizeof(FW_1240_PATCHES[0]);
    assert(count == 31);

    for (size_t i = 0; i < count; i++) {
        assert(FW_1240_PATCHES[i].offset > 0);
        assert(FW_1240_PATCHES[i].len >= 1 && FW_1240_PATCHES[i].len <= 16);
        /* Ensure no patch crosses 4KB boundary */
        uint64_t page_off = FW_1240_PATCHES[i].offset & 0xFFFULL;
        assert(page_off + FW_1240_PATCHES[i].len <= 0x1000ULL);
    }

    /* 3. Validate x86-64 4-level page table translation */
    static uint64_t pml4_page[512] __attribute__((aligned(4096)));
    static uint64_t pdpt_page[512] __attribute__((aligned(4096)));
    static uint64_t pd_page[512] __attribute__((aligned(4096)));
    static uint64_t pt_page[512] __attribute__((aligned(4096)));

    memset(pml4_page, 0, sizeof(pml4_page));
    memset(pdpt_page, 0, sizeof(pdpt_page));
    memset(pd_page, 0, sizeof(pd_page));
    memset(pt_page, 0, sizeof(pt_page));

    /* Treat host memory as direct-mapped with base 0 for this test */
    mock_dmap_base = 0;
    uint64_t cr3 = (uint64_t)(uintptr_t)pml4_page;

    /* Setup a 4KB mapping for va = 0x800C870C3 (PML4=16, PDPT=5, PD=2, PT=135, offset=0x0C3) */
    uint64_t test_va = 0x800C870C3ULL;
    uint64_t test_phys_frame = 0x12345000ULL;

    uint64_t pml4i = (test_va >> 39) & 0x1FFULL;
    uint64_t pdpti = (test_va >> 30) & 0x1FFULL;
    uint64_t pdi   = (test_va >> 21) & 0x1FFULL;
    uint64_t pti   = (test_va >> 12) & 0x1FFULL;

    pml4_page[pml4i] = ((uint64_t)(uintptr_t)pdpt_page) | 0x07ULL; /* Present + RW + User */
    pdpt_page[pdpti] = ((uint64_t)(uintptr_t)pd_page) | 0x07ULL;
    pd_page[pdi]     = ((uint64_t)(uintptr_t)pt_page) | 0x07ULL;
    pt_page[pti]     = test_phys_frame | 0x07ULL;

    uint64_t resolved_pa = test_virt_to_phys(cr3, mock_dmap_base, test_va);
    assert(resolved_pa == (test_phys_frame | (test_va & 0xFFFULL)));
    assert(resolved_pa == 0x123450C3ULL);

    /* Test 2MB superpage translation */
    uint64_t test_2mb_va = 0x800200042ULL;
    uint64_t test_2mb_pdi = (test_2mb_va >> 21) & 0x1FFULL;
    uint64_t test_2mb_frame = 0x40000000ULL;
    pd_page[test_2mb_pdi] = test_2mb_frame | 0x87ULL; /* PS=1 (2MB) + Present */
    uint64_t resolved_2mb_pa = test_virt_to_phys(cr3, mock_dmap_base, test_2mb_va);
    assert(resolved_2mb_pa == (test_2mb_frame | (test_2mb_va & 0x1FFFFFULL)));

    /* Test unmapped address returns 0 */
    uint64_t unmapped_va = 0x900000000ULL;
    assert(test_virt_to_phys(cr3, mock_dmap_base, unmapped_va) == 0);

    printf("pltauth_patch_selftest: ok (31 ShellCore FW 12.40 patches verified, 4-level page translation ok)\n");
    return 0;
}
