#ifndef RUNTIME_EXTERNS_H
#define RUNTIME_EXTERNS_H

extern "C" int Lread();
extern "C" int Lwrite(int n);
extern "C" int Llength(void *p);
extern "C" void *Lstring(void *p);

extern "C" void *Bstring(void *p);
extern "C" int LtagHash(char *s);
extern "C" void *Bsta(void *v, int i, void *x);
extern "C" void *Belem(void *p, int i);
extern "C" int Btag(void *d, int t, int n);
extern "C" int Barray_patt(void *d, int n);

extern "C" int Bstring_patt(void *x, void *y);
extern "C" int Bclosure_tag_patt(void *x);
extern "C" int Bboxed_patt(void *x);
extern "C" int Bunboxed_patt(void *x);
extern "C" int Barray_tag_patt(void *x);
extern "C" int Bstring_tag_patt(void *x);
extern "C" int Bsexp_tag_patt(void *x);

extern "C" void Bmatch_failure(void *v, char *fname, int line, int col);

extern "C" void *alloc_array(int);
extern "C" void *alloc_sexp(int);
extern "C" void *alloc_closure(int);

extern "C" void __init();
extern "C" void __shutdown();

inline void *Barray(int n);

inline void *BSexp(int n, int tag);

inline void *Bclosure(int n, void *entry);

#endif // RUNTIME_EXTERNS_H
