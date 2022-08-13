#include <stdio.h>
#include <zlib.h>
#include "mppriv.h"
#include "kalloc.h"
#include "kseq.h"
KSEQ_INIT(gzFile, gzread)

mp_ntdb_t *mp_ntseq_read(const char *fn)
{
	gzFile fp;
	kseq_t *ks;
	mp_ntdb_t *d = 0;
	int64_t off = 0;

	fp = gzopen(fn, "r");
	if (fp == 0) return 0;
	ks = kseq_init(fp);

	d = Kcalloc(0, mp_ntdb_t, 1);
	while (kseq_read(ks) >= 0) {
		int64_t i, ltmp;
		mp_ctg_t *c;

		// update mp_ntdb_t::ctg
		if (d->n_ctg == d->m_ctg) {
			d->m_ctg += (d->m_ctg>>1) + 16;
			d->ctg = Krealloc(0, mp_ctg_t, d->ctg, d->m_ctg);
		}
		c = &d->ctg[d->n_ctg++];
		c->name = mp_strdup(ks->name.s);
		c->off = off;
		c->len = ks->seq.l;

		// update mp_ntdb_t::seq
		ltmp = (d->l_seq + ks->seq.l + 1) >> 1 << 1;
		if (ltmp > d->m_seq) {
			int64_t oldm = d->m_seq;
			d->m_seq = ltmp;
			kroundup64(d->m_seq);
			d->seq = Krealloc(0, uint8_t, d->seq, d->m_seq);
			memset(&d->seq[oldm>>1], 0, (d->m_seq - oldm) >> 1);
		}
		for (i = 0; i < ks->seq.l; ++i, ++off) {
			uint8_t b = mp_tab_nt4[(uint8_t)ks->seq.s[i]];
			d->seq[off >> 1] |= b << (off&1) * 4;
		}
	}

	kseq_destroy(ks);
	gzclose(fp);
	return d;
}

int64_t mp_ntseq_get(const mp_ntdb_t *db, int32_t cid, int64_t st, int64_t en, int32_t rev, uint8_t *seq)
{
	int64_t i, s, e, k;
	if (cid >= db->n_ctg || cid < 0) return -1;
	if (en < 0 || en > db->ctg[cid].len) en = db->ctg[cid].len;
	s = db->ctg[cid].off + st;
	e = db->ctg[cid].off + en;
	if (!rev) {
		for (i = s, k = 0; i < e; ++i)
			seq[k++] = db->seq[i>>1] >> ((i&1) * 4) & 0xf;
	} else {
		for (i = e - 1, k = 0; i >= s; --i) {
			uint8_t c = db->seq[i>>1] >> ((i&1) * 4) & 0xf;
			seq[k++] = c >= 4? c : 3 - c;
		}
	}
	return k;
}

static inline void mp_ntseq_process_orf(uint8_t phase, int64_t st, int64_t en)
{
	if (en - st >= 50)
		printf("%ld\t%ld\n", (long)st, (long)en);
}

void mp_ntseq_get_orf(int64_t len, const uint8_t *seq)
{
	uint8_t codon[3], p;
	int64_t i, e[3], k[3], l[3];
	for (i = 0; i < 3; ++i)
		e[i] = -1, k[i] = l[i] = 0, codon[i] = 0;
	for (i = 0, p = 0; i < len; ++i, ++p) {
		if (p == 3) p = 0;
		if (seq[i] < 4) {
			codon[p] = (codon[p] << 2 | seq[i]) & 0x3f;
			if (++l[p] >= 3) {
				uint8_t aa = mp_tab_codon[(uint8_t)codon[p]];
				if (aa >= 20) {
					mp_ntseq_process_orf(p, e[p] + 1 - k[p] * 3, e[p] + 1);
					k[p] = l[p] = 0, e[p] = -1;
				} else e[p] = i, ++k[p];
			}
		} else {
			mp_ntseq_process_orf(p, e[p] + 1 - k[p] * 3, e[p] + 1);
			k[p] = l[p] = 0, e[p] = -1;
		}
	}
	for (i = 0; i < 3; ++i)
		mp_ntseq_process_orf(i, e[p] + 1 - k[p] * 3, e[p] + 1);
}
