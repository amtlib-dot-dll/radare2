/* radare - LGPL - Copyright 2009-2015 - pancake, nibble */

#include <r_anal.h>
#include <r_util.h>
#include <r_list.h>
#include <r_io.h>
#include "../config.h"

R_LIB_VERSION(r_anal);

static RAnalPlugin *anal_static_plugins[] =
	{ R_ANAL_STATIC_PLUGINS };

static void r_anal_type_init(RAnal *anal) {
	Sdb *D = anal->sdb_types;
	sdb_set (D, "unsigned int", "type", 0);
	sdb_set (D, "unsigned char", "type", 0);
	sdb_set (D, "unsigned short", "type", 0);
	sdb_set (D, "int", "type", 0);
	sdb_set (D, "long", "type", 0);
	sdb_set (D, "void *", "type", 0);
	sdb_set (D, "char", "type", 0);
	sdb_set (D, "char *", "type", 0);
	sdb_set (D, "const char*", "type", 0);
	sdb_set (D, "uint8_t", "type", 0);
	sdb_set (D, "uint16_t", "type", 0);
	sdb_set (D, "uint32_t", "type", 0);
	sdb_set (D, "uint64_t", "type", 0);
	sdb_set (D, "type.unsigned int", "i", 0);
	sdb_set (D, "type.unsigned char", "b", 0);
	sdb_set (D, "type.unsigned short", "w", 0);
	sdb_set (D, "type.int", "d", 0);
	sdb_set (D, "type.long", "x", 0);
	sdb_set (D, "type.void *", "p", 0);
	sdb_set (D, "type.char", "b", 0);
	sdb_set (D, "type.char *", "*z", 0);
	sdb_set (D, "type.const char*", "*z", 0);
	sdb_set (D, "type.uint8_t", "b", 0);
	sdb_set (D, "type.uint16_t", "w", 0);
	sdb_set (D, "type.uint32_t", "d", 0);
	sdb_set (D, "type.uint64_t", "q", 0);
}

R_API void r_anal_set_limits(RAnal *anal, ut64 from, ut64 to) {
	free (anal->limit);
	anal->limit = R_NEW0 (RAnalRange);
	if (anal->limit) {
		anal->limit->from = from;
		anal->limit->to = to;
	}
}

R_API void r_anal_unset_limits(RAnal *anal) {
	free (anal->limit);
	anal->limit = NULL;
}

static void meta_unset_for(void *user, int idx) {
	RSpaces *s = (RSpaces*)user;
	RAnal *anal = (RAnal*)s->user;
	r_meta_space_unset_for (anal, idx);
}

static int meta_count_for(void *user, int idx) {
	int ret;
	RSpaces *s = (RSpaces*)user;
	RAnal *anal = (RAnal*)s->user;
	ret = r_meta_space_count_for (anal, idx);
	return ret;
}

R_API RAnal *r_anal_new() {
	int i;
	RAnalPlugin *static_plugin;
	RAnal *anal = R_NEW0 (RAnal);
	if (!anal) return NULL;
	anal->reflines = anal->reflines2 = NULL;
	anal->esil_goto_limit = R_ANAL_ESIL_GOTO_LIMIT;
	anal->limit = NULL;
	anal->opt.nopskip = R_TRUE; // skip nops in code analysis
	anal->decode = R_TRUE; // slow slow if not used
	anal->gp = 0LL;
	anal->sdb = sdb_new0 ();
	anal->opt.noncode = R_FALSE; // do not analyze data by default
	anal->sdb_fcns = sdb_ns (anal->sdb, "fcns", 1);
	anal->sdb_meta = sdb_ns (anal->sdb, "meta", 1);
	r_space_init (&anal->meta_spaces,
		meta_unset_for, meta_count_for, anal);
	anal->sdb_hints = sdb_ns (anal->sdb, "hints", 1);
	anal->sdb_xrefs = sdb_ns (anal->sdb, "xrefs", 1);
	anal->sdb_types = sdb_ns (anal->sdb, "types", 1);
	anal->cb_printf = (PrintfCallback) printf;
	r_anal_pin_init (anal);
	r_anal_type_init (anal);
	r_anal_xrefs_init (anal);
	anal->diff_thbb = R_ANAL_THRESHOLDBB;
	anal->diff_thfcn = R_ANAL_THRESHOLDFCN;
	anal->split = R_TRUE; // used from core
	anal->syscall = r_syscall_new ();
	r_io_bind_init (anal->iob);
	r_flag_bind_init (anal->flb);
	anal->reg = r_reg_new ();
	anal->lineswidth = 0;
	anal->fcns = r_anal_fcn_list_new ();
#if USE_NEW_FCN_STORE
	anal->fcnstore = r_listrange_new ();
#endif
	anal->refs = r_anal_ref_list_new ();
	anal->types = r_anal_type_list_new ();
	r_anal_set_bits (anal, 32);
	r_anal_set_big_endian (anal, R_FALSE);
	anal->plugins = r_list_new ();
	anal->plugins->free = (RListFree) r_anal_plugin_free;
	for (i=0; anal_static_plugins[i]; i++) {
		static_plugin = R_NEW (RAnalPlugin);
		*static_plugin = *anal_static_plugins[i];
		r_anal_add (anal, static_plugin);
	}

	return anal;
}

R_API void r_anal_plugin_free (RAnalPlugin *p) {
	if (p && p->fini) {
		p->fini (p);
	}
}

R_API RAnal *r_anal_free(RAnal *a) {
	if (!a) return NULL;
	/* TODO: Free anals here */
	R_FREE (a->cpu);
	r_list_free (a->plugins);
	a->fcns->free = r_anal_fcn_free;
	r_list_free (a->fcns);
	r_space_fini (&a->meta_spaces);
	r_anal_pin_fini (a);
	r_list_free (a->refs);
	r_list_free (a->types);
	r_reg_free (a->reg);
	r_anal_op_free (a->queued);
	a->sdb = NULL;
	r_syscall_free (a->syscall);
	sdb_ns_free (a->sdb);
	if (a->esil) {
		r_anal_esil_free (a->esil);
		a->esil = NULL;
	}
	memset (a, 0, sizeof (RAnal));
	free (a);
	return NULL;
}

R_API void r_anal_set_user_ptr(RAnal *anal, void *user) {
	anal->user = user;
}

R_API int r_anal_add(RAnal *anal, RAnalPlugin *foo) {
	if (foo->init)
		foo->init (anal->user);
	r_list_append (anal->plugins, foo);
	return R_TRUE;
}

// TODO: Must be deprecated
R_API int r_anal_list(RAnal *anal) {
	RAnalPlugin *h;
	RListIter *it;
	r_list_foreach (anal->plugins, it, h) {
		anal->cb_printf ("anal %-10s %s\n", h->name, h->desc);
	}
	return R_FALSE;
}

R_API int r_anal_use(RAnal *anal, const char *name) {
	RListIter *it;
	RAnalPlugin *h;
	r_list_foreach (anal->plugins, it, h) {
		if (!strcmp (h->name, name)) {
			anal->cur = h;
			r_anal_set_reg_profile (anal);
			if (anal->esil) {
				r_anal_esil_free (anal->esil);
				anal->esil = NULL;
			}
			return R_TRUE;
		}
	}
	return R_FALSE;
}

R_API int r_anal_set_reg_profile(RAnal *anal) {
	if (anal && anal->cur && anal->cur->set_reg_profile)
		return anal->cur->set_reg_profile (anal);
	return R_FALSE;
}

R_API int r_anal_set_bits(RAnal *anal, int bits) {
	switch (bits) {
	case 8:
	case 16:
	case 32:
	case 64:
		anal->bits = bits;
		r_anal_set_reg_profile (anal);
		return R_TRUE;
	}
	return R_FALSE;
}

R_API void r_anal_set_cpu(RAnal *anal, const char *cpu) {
	free (anal->cpu);
	anal->cpu = cpu ? strdup (cpu) : NULL;
}

R_API int r_anal_set_big_endian(RAnal *anal, int bigend) {
	anal->big_endian = bigend;
	anal->reg->big_endian = bigend;
	return R_TRUE;
}

R_API char *r_anal_strmask (RAnal *anal, const char *data) {
	RAnalOp *op;
	ut8 *buf;
	char *ret = NULL;
	int oplen, len, idx = 0;

	ret = strdup (data);
	buf = malloc (1+strlen (data));
	op = r_anal_op_new ();
	if (op == NULL || ret == NULL || buf == NULL) {
		free (op);
		free (buf);
		free (ret);
		return NULL;
	}
	len = r_hex_str2bin (data, buf);
	while (idx < len) {
		if ((oplen = r_anal_op (anal, op, 0, buf+idx, len-idx)) <1)
			break;
		switch (op->type) {
		case R_ANAL_OP_TYPE_CALL:
		case R_ANAL_OP_TYPE_UCALL:
		case R_ANAL_OP_TYPE_CJMP:
		case R_ANAL_OP_TYPE_JMP:
		case R_ANAL_OP_TYPE_UJMP:
			if (op->nopcode != 0)
				memset (ret+(idx+op->nopcode)*2,
					'.', (oplen-op->nopcode)*2);
		}
		idx += oplen;
	}
	free (op);
	free (buf);
	return ret;
}

R_API void r_anal_trace_bb(RAnal *anal, ut64 addr) {
	RAnalBlock *bbi;
	RAnalFunction *fcni;
	RListIter *iter2;
#define OLD 0
#if OLD
	RListIter *iter;
	r_list_foreach (anal->fcns, iter, fcni) {
		r_list_foreach (fcni->bbs, iter2, bbi) {
			if (addr>=bbi->addr && addr<(bbi->addr+bbi->size)) {
				bbi->traced = R_TRUE;
				break;
			}
		}
	}
#else
	fcni = r_anal_get_fcn_in (anal, addr, 0);
	if (fcni) {
		r_list_foreach (fcni->bbs, iter2, bbi) {
			if (addr>=bbi->addr && addr<(bbi->addr+bbi->size)) {
				bbi->traced = R_TRUE;
				break;
			}
		}
	}
#endif
}

R_API RList* r_anal_get_fcns (RAnal *anal) {
	// avoid received to free this thing
	anal->fcns->free = NULL;
	return anal->fcns;
}

R_API int r_anal_project_load(RAnal *anal, const char *prjfile) {
	if (prjfile && *prjfile)
		return r_anal_xrefs_load (anal, prjfile);
	return R_FALSE;
}

R_API int r_anal_project_save(RAnal *anal, const char *prjfile) {
	if (!prjfile || !*prjfile)
		return R_FALSE;
	r_anal_xrefs_save (anal, prjfile);
	return R_TRUE;
}

R_API RAnalOp *r_anal_op_hexstr(RAnal *anal, ut64 addr, const char *str) {
	int len;
	ut8 *buf;
	RAnalOp *op = R_NEW0 (RAnalOp);
	if (!op) return NULL;
	buf = malloc (strlen (str)+1);
	if (!buf) {
		free (op);
		return NULL;
	}
	len = r_hex_str2bin (str, buf);
	r_anal_op (anal, op, addr, buf, len);
	return op;
}

R_API int r_anal_op_is_eob (RAnalOp *op) {
	switch (op->type) {
	case R_ANAL_OP_TYPE_JMP:
	case R_ANAL_OP_TYPE_UJMP:
	case R_ANAL_OP_TYPE_CJMP:
	case R_ANAL_OP_TYPE_RET:
	case R_ANAL_OP_TYPE_TRAP:
		return 1;
	}
	return 0;
}

R_API int r_anal_purge (RAnal *anal) {
	sdb_reset (anal->sdb_fcns);
	sdb_reset (anal->sdb_meta);
	sdb_reset (anal->sdb_hints);
	sdb_reset (anal->sdb_xrefs);
	sdb_reset (anal->sdb_types);
	r_list_free (anal->fcns);
	anal->fcns = r_anal_fcn_list_new ();
#if USE_NEW_FCN_STORE
	r_listrange_free (anal->fcnstore);
	anal->fcnstore = r_listrange_new ();
#endif
	r_list_free (anal->refs);
	anal->refs = r_anal_ref_list_new ();
	r_list_free (anal->types);
	anal->types = r_anal_type_list_new ();
	return 0;
}
