#include <string>
using std::string;
#include <unordered_map>
using std::unordered_map;
#include <iostream>
#include <fstream>
#include <streambuf>

#define assert(E) if (!(E)) { std::cerr << "assert at " << __LINE__ << std::endl; rexit(-1);}

void rexit(int i) {
  exit(i);
}

bool trace_gc = false;
bool trace_eval = false;
bool cont_passing_mode = true;

// -- allocator
const size_t kSlotsCount = 8192;
const size_t tVal = kSlotsCount;
const size_t tNum = tVal;
const size_t tSymbol = tVal + 1;
const size_t tFree = tVal + 2;

struct Var{
  size_t h;
  union {
    size_t t; // if h < tVal
    int v;    // if h == tNum
    string* s_name; // if h == tSymbol
  };
};

Var vars [kSlotsCount];
size_t max_var = 0;
size_t allocated_count, first_free;
unordered_map<string, size_t> symbols;

void reset_allocator() {
  symbols.clear();
  symbols["nil"] = 0;
  for (size_t i = 1; i <= max_var; i++)
    if (vars[i].h == tSymbol)
      delete vars[i].s_name;
  max_var = allocated_count = first_free = 0;
}

size_t alloc_var() {
  allocated_count++;
  if (first_free)
  {
    size_t r = first_free;
    first_free = vars[first_free].t;
    return r;
  }
  assert(max_var < kSlotsCount - 1);
  return ++max_var;
}

void free_var(size_t v) {
    allocated_count--;
    if (vars[v].h == tSymbol)
        delete vars[v].s_name;
    vars[v].h = tFree;
    vars[v].t = first_free;
    first_free = v;
}

int get_int(size_t v) { return vars[v].h == tNum ? vars[v].v : 0; }
size_t h(size_t v) { return vars[v].h < tVal ? vars[v].h : 0; }
size_t t(size_t v) { return vars[v].h < tVal ? vars[v].t : 0; }

size_t mk_int(int v)
{
  size_t r = alloc_var();
  vars[r].h = tNum;
  vars[r].v = v;
  return r;
}

size_t get_symbol(const string& name) {
  auto a = symbols.find(name);
  if (a != symbols.end())
    return a->second;
  size_t r = alloc_var();
  vars[r].h = tSymbol;
  vars[r].s_name = new string(name);
  return symbols[name] = r;
}
size_t mk_pair(size_t h, size_t t) {
  size_t r = alloc_var();
  vars[r].h = h;
  vars[r].t = t;
  return r;
}

void allocator_test() {
  reset_allocator();
  size_t pair, a1, i2;
  size_t root = mk_pair(
    a1 = get_symbol("test"),
    pair = mk_pair(
      get_symbol("test"),
      i2 = mk_int(42)));
  assert(allocated_count == 4);
  assert(get_int(i2) == 42);
  assert(h(pair) == a1);
  assert(h(root) == a1);
  assert(t(root) == pair);
  free_var(root);
  assert(allocated_count == 3);
  assert(mk_pair(0, 0) == root);
  reset_allocator();
}

// -- GC

const auto kMark = ~(~size_t(0) >> 1);

void gc_mark(size_t i) {
  while (i) {
    size_t h = vars[i].h;
    vars[i].h |= kMark;
    if (h & kMark || h >= tVal)
      return;
    gc_mark(h);
    i = vars[i].t;
  }
}

void gc_sweep() {
  int freed_cnt = 0;
  for (size_t i = 1; i <= max_var; i++) {
    if (vars[i].h & kMark)
      vars[i].h &= ~kMark;
    else if (vars[i].h != tFree) {
      free_var(i);
      if (trace_gc)
        freed_cnt++;
    }
  }
  if (trace_gc)
    std::cout << "gc: freed " << freed_cnt << std::endl;
}

void gc_test() {
  reset_allocator();
  size_t root = mk_pair(
    get_symbol("test"),
    mk_pair(
      get_symbol("test"),
      mk_int(42)));
  gc_mark(root);
  gc_sweep();
  assert(allocated_count == 4);
  vars[root].t = 0;
  gc_mark(root);
  gc_sweep();
  assert(allocated_count == 2);
  gc_sweep();
  assert(allocated_count == 0);
  reset_allocator();
}

// -- visualization

const size_t kDoubleMark = kMark >> 1;

void format_mark_refs(size_t i) {
  while (i) {
    size_t h = vars[i].h;
    if (h & kMark)
      vars[i].h |= kDoubleMark;
    if (h >= tVal)
      break;
    vars[i].h |= kMark;
    format_mark_refs(h);
    i = vars[i].t;
  }
}

string name_of(size_t i) {
  string r;
  do
    r += 'a' + i % ('z' - 'a');
  while ((i /= 'z' - 'a') != 0);
  return r;
}

string format_rec(size_t i) {
  if (!i) return ".";
  if (vars[i].h == tNum) return std::to_string(vars[i].v);
  if (vars[i].h == tSymbol) return *vars[i].s_name;
  if (!(vars[i].h & kMark)) return "#" + name_of(i);
  string r;
  if (vars[i].h & kDoubleMark) r += name_of(i) + ":";
  r += '(';
  do {
    vars[i].h &= ~(kMark | kDoubleMark);
    r += format_rec(vars[i].h);
    r += ' ';
    i = vars[i].t;
  } while (i && !(vars[i].h & kDoubleMark) && (vars[i].h & kMark));
  return r + format_rec(i) + ')';
}

string format(size_t i) {
  format_mark_refs(i);
  return format_rec(i);
}

void visualization_test() {
  reset_allocator();
  assert(format(0) == ".");
  assert(format(mk_int(42)) == "42");
  assert(format(get_symbol("test")) == "test");
  assert(format(mk_pair(
    mk_int(1), mk_pair(
      mk_int(2),
      mk_pair(mk_int(3), 0)))) == "(1 2 3 .)");
  assert(format(mk_pair(
    mk_pair(mk_int(1), mk_int(2)),
    mk_pair(mk_int(3), 0))) == "((1 2) 3 .)");
  reset_allocator();
  size_t a = mk_pair(mk_int(1), mk_int(2));
  assert(format(mk_pair(a, a)) == "(d:(1 2) #d)");
  vars[a].t = a;
  assert(format(mk_pair(0, a)) == "(. d:(1 #d))");
}

// -- parsing

void skip_ws(const char*& pos) {
  while (*pos && *pos <= ' ')
    pos++;
}

bool is_num(char c) { return c >= '0' && c <= '9'; }
const char error_marker = 0;
const char * last_open_par;

size_t parse(const char*& pos) {
  skip_ws(pos);
  if (*pos == '(') {
    last_open_par = pos;
    pos++;
    size_t r = 0;
    for (size_t *d = &r; *pos !=')'; d = &vars[*d].t) {
      if (!*pos) {
        pos = &error_marker;
        return 0;
      }
      *d = mk_pair(parse(pos), 0);
    }
    pos++;
    return r;
  }
  if (is_num(*pos)) {
    int r = 0;
    while (is_num(*pos))
      r = r * 10 + *pos++ - '0';
    return mk_int(r);
  } 
  string r;
  while (*pos > ' ' && *pos != '(' && *pos != ')')
    r += *pos++;
  return get_symbol(r);
}

void parsing_test() {
  //size_t ctx = reset_global_ctx();
  const char *pos;
  assert(format(parse(pos = "(- 3 1)")) == "(- 3 1 .)");
  assert(pos != &error_marker && !*pos);
  assert(format(parse(pos = "(((a b) + a b) 2 3)")) == "(((a b .) + a b .) 2 3 .)");
  assert(pos != &error_marker && !*pos);
}


// -- global_ctx

enum {
  tNil,
  tLit, tIf, tAdd, tSub, tMul, tLt, tEq, tCon, tHead, tTail,
  tLambda, tLet, tLetRec, // not used in continuation passing
  tUser, // first user defined pair
};

size_t reset_global_ctx() {
  const char* const builtins[] = {"'", "?", "+", "-", "*", "<", "=", ".", "head", "tail", "lambda", "let", "letrec"};
  reset_allocator();
  size_t global_ctx = 1;
  for (const auto n: builtins)
    get_symbol(n);
  for (const auto n: builtins){
    size_t a = get_symbol(n);
    global_ctx = mk_pair(mk_pair(a, a), global_ctx);
  }
  return global_ctx;
}

size_t lookup(size_t symbol, size_t ctx) {
  for (; ctx; ctx = t(ctx)) {
    size_t a = h(ctx);
    if (h(a) == symbol)
      return t(a);
  }
  std::cerr << "unknown symbol " << (vars[symbol].h == tSymbol ? vars[symbol].s_name->c_str() : "???") << std::endl;
  return 0;
}

void global_ctx_test() {
  size_t ctx = reset_global_ctx();
  assert(lookup(get_symbol("'"), ctx) == get_symbol("'"));
  assert(lookup(get_symbol("tail"), ctx) == get_symbol("tail"));
}

// -- evaluation with continuation passing

size_t eval_param(size_t n, size_t ctx) {
  return !n || vars[n].h == tNum ? n :
    vars[n].h == tSymbol ? lookup(n, ctx) :
    h(n) == tLit ? t(n) :
    mk_pair(ctx, n);
}

void jmp(size_t& n, size_t &ctx, size_t val) {
  size_t cont = eval_param(h(t(t(t(n)))), ctx);
  ctx = mk_pair(mk_pair(h(h(t(cont))), val), h(cont));
  n = t(t(cont));
}

size_t cont_eval(size_t n, size_t ctx) {
  for (;;)
  {
    if (kSlotsCount - allocated_count < 20) {
      gc_mark(n);
      gc_mark(ctx);
      gc_sweep();
    }
    if (trace_eval) {
      for (size_t c = ctx; c; c = t(c))
        std::cout << "  " << format(h(h(c))) << " = " << format(t(h(c))) << "\n";
      std::cout << "f: " << format(n) << std::endl;
    }
    size_t fn = eval_param(h(n), ctx);
    switch (fn) // if (builtin_symbol params cont)
    {
      case tNil: return t(n) ? eval_param(h(t(n)), ctx) : t(h(ctx)); // value of last fn parameter
      case tIf:
        fn = eval_param(h(t(t(eval_param(h(t(n)), ctx) ? n : t(n)))), ctx);
        ctx = h(fn);
        n = t(t(fn));
        if (!n) return fn;
        continue;
      case tAdd: jmp(n, ctx, mk_int(get_int(eval_param(h(t(n)), ctx)) + get_int(eval_param(h(t(t(n))), ctx)))); continue;
      case tSub: jmp(n, ctx, mk_int(get_int(eval_param(h(t(n)), ctx)) - get_int(eval_param(h(t(t(n))), ctx)))); continue;
      case tMul: jmp(n, ctx, mk_int(get_int(eval_param(h(t(n)), ctx)) * get_int(eval_param(h(t(t(n))), ctx)))); continue;
      case tLt: jmp(n, ctx, get_int(eval_param(h(t(n)), ctx)) < get_int(eval_param(h(t(t(n))), ctx)) ? n : tNil); continue;
      case tEq: jmp(n, ctx, get_int(eval_param(h(t(n)), ctx)) == get_int(eval_param(h(t(t(n))), ctx)) ? n : tNil); continue;
      case tCon: jmp(n, ctx, mk_pair(eval_param(h(t(n)), ctx), eval_param(h(t(t(n))), ctx))); continue;
      case tHead:
        fn = eval_param(h(t(t(n))), ctx);
        ctx = mk_pair(mk_pair(h(h(t(fn))), h(eval_param(h(t(n)), ctx))), h(fn));
        n = t(t(fn));
        continue;
      case tTail:
        fn = eval_param(h(t(t(n))), ctx);
        ctx = mk_pair(mk_pair(h(h(t(fn))), t(eval_param(h(t(n)), ctx))), h(fn));
        n = t(t(fn));
        continue;
    }
    if (vars[fn].h == tNum)
      return fn;
    size_t callee_ctx = h(fn);    // (fn params), fn is (ctx (param_names) delegate_fn dfn param)
    for (size_t actual = t(n), formal = h(t(fn)); actual && formal; actual = t(actual), formal = t(formal))
      callee_ctx = mk_pair(mk_pair(h(formal), eval_param(h(actual), ctx)), callee_ctx);
    ctx = callee_ctx;
    n = t(t(fn));
  }
}

int cont_compile_eval(const char* s) {
  size_t ctx = reset_global_ctx();
  size_t fn = parse(s);
  return s == &error_marker || *s ? printf("error at %s", s), -1 : get_int(cont_eval(fn, ctx));
}

// -- classic evaluation

struct gc_guard{
  static gc_guard* root;
  size_t f = 0, ctx = 0, temp = 0, temp1 = 0;
  gc_guard*prev;
  gc_guard() : prev(root) { root = this; }
  ~gc_guard() { root = prev; }
  void set(size_t f, size_t ctx) {
    this->f = f;
    this->ctx = ctx;
    this->temp = this->temp = 0;
  }
};
gc_guard* gc_guard::root = nullptr;

size_t eval(size_t n, size_t ctx) {
  gc_guard guard;
  for (;;) {
    guard.set(n, ctx);
    if (kSlotsCount - allocated_count < 20) {
      for (gc_guard* i = gc_guard::root; i ; i = i->prev) {
        gc_mark(i->f);
        gc_mark(i->ctx);
        gc_mark(i->temp);
        gc_mark(i->temp1);
      }
      gc_sweep();
    }
    if (trace_eval) {
      for (size_t c = ctx; c; c = t(c))
        std::cout << "  " << format(h(h(c))) << " = " << format(t(h(c))) << "\n";
      std::cout << "f: " << format(n) << std::endl;
    }
    if (!n || vars[n].h == tNum) return n;
    if (vars[n].h == tSymbol) return lookup(n, ctx);
    size_t fn = guard.temp = eval(h(n), ctx);
    switch (fn) {
    case tLit: return t(n);
    case tIf: n = h(t(t(eval(h(t(n)), ctx) ? n : t(n)))); continue;
    case tAdd: return mk_int(get_int(eval(h(t(n)), ctx)) + get_int(eval(h(t(t(n))), ctx)));
    case tSub: return mk_int(get_int(eval(h(t(n)), ctx)) - get_int(eval(h(t(t(n))), ctx)));
    case tMul: return mk_int(get_int(eval(h(t(n)), ctx)) * get_int(eval(h(t(t(n))), ctx)));
    case tLt: return get_int(eval(h(t(n)), ctx)) < get_int(eval(h(t(t(n))), ctx)) ? n : 0;
    case tEq: return get_int(eval(h(t(n)), ctx)) == get_int(eval(h(t(t(n))), ctx)) ? n : 0;
    case tCon:
      guard.temp = eval(h(t(n)), ctx);
      return mk_pair(guard.temp, eval(h(t(t(n))), ctx));
    case tHead: return h(eval(h(t(n)), ctx));
    case tTail: return t(eval(h(t(n)), ctx));
    case tLambda: return mk_pair(ctx, t(n));
    case tLet: // (let name initializer body)
      ctx = mk_pair(
          mk_pair(h(t(n)),
            eval(h(t(t(n))), ctx)),
          ctx);
      n = h(t(t(t(n))));
      continue;
    case tLetRec:
      ctx = guard.temp = mk_pair(mk_pair(h(t(n)), 0), ctx);
      vars[h(ctx)].t = eval(h(t(t(n))), ctx);
      n = h(t(t(t(n))));
      continue;
    }
    size_t callee_ctx = h(fn);    // (fn params), fn is (ctx (param_names) body)
    for (size_t actual = t(n), formal = h(t(fn)); actual && formal; actual = t(actual), formal = t(formal))
      callee_ctx = guard.temp1 = mk_pair(mk_pair(h(formal), eval(h(actual), ctx)), callee_ctx);
    ctx = callee_ctx;
    n = h(t(t(fn)));
  }
}

int compile_eval(const char* s) {
  size_t ctx = reset_global_ctx();
  size_t fn = parse(s);
  return s == &error_marker || *s ? printf("error at %s", s), -1 : get_int(eval(fn, ctx));
}

void cont_eval_test() {
  assert(2 == cont_compile_eval("(- 3 1)"));
  assert(4 == cont_compile_eval("(- 3 1 ((x) + x x))")); // x=3-1; x+x
  assert(5 == cont_compile_eval("(((a b) + a b) 2 3)")); // ((lambda (a b) (+ a b)) 2 3)
  assert(5 == cont_compile_eval("(< 3 1 ((a) ? a 2 5))")); // if 3<1 :2 :5
  assert(4 == cont_compile_eval(R"-(
    (((len) len (' 1 2 3 4) nil)
      ((list r)
        ((lnrec) lnrec 0  list lnrec r)
        ((c l f r)
          ? l
            (() tail l ((tl) + 1 c ((inc) f inc tl f r)))
            (() r c))
      )
    ))-"));
}

void eval_test() {
  assert(2 == compile_eval("(- 3 1)"));
  assert(4 == compile_eval("(let x (- 3 1) (+ x x))"));
  assert(5 == compile_eval("((lambda (a b) (+ a b)) 2 3)"));
  assert(5 == compile_eval("(? (< 3 1) 2 5)"));
  assert(4 == compile_eval(R"-(
    (letrec len
      (lambda (l)
        (? l
          (+ 1 (len (tail l)))
          0
        )
      )
      (len (' 1 2 3 4))
    ))-"));
  assert(4 == compile_eval(R"-(
    (let len
      (lambda (list)
        (letrec len_r
          (lambda (c l)
            (? l
              (len_r (+ 1 c) (tail l))
              c
            )
          )
          (len_r 0 list)
        )
      )
      (len (' 1 2 3 4))
    ))-"));
}

void show_help() {
   std::cout <<
     "little-lisp" << std::endl <<
     "usage: ll -flags \"expression or filename\"" << std::endl <<
     "where flags are:" << std::endl <<
     "  t - run self tests" << std::endl <<
     "  h - this help" << std::endl <<
     "  g - show gc statistics" << std::endl <<
     "  v - varbose evaluation trace" << std::endl <<
     std::endl <<
     "  p - continuation passing mode (default)" << std::endl <<
     "  c - or classic mode" << std::endl <<
     std::endl <<
     "  r - return value as errorlevel" << std::endl <<
     "  o - or return value to stdout (default)" << std::endl <<
     std::endl <<
     "  i - command line is an expression (default)" << std::endl <<
     "  f - or command line is a file name" << std::endl;
}

int main(int param_cnt, const char* const* params) {
  if (param_cnt < 2) {
    std::cerr << "help: ll -h" << std::endl;
    rexit(1);
  }
  bool immediate_mode = true;
  bool to_result_code = false;
  while (param_cnt > 1 && *params[1] == '-') {
    params++;
    for (const char* p = *params; *++p;) {
      switch(*p) {
      case 't':
        allocator_test();
        gc_test();
        global_ctx_test();
        visualization_test();
        parsing_test();
        eval_test();
        cont_eval_test();
        std::cout << "tests passed" << std::endl;
        rexit(1);
      case 'g': trace_gc = true; break;
      case 'v': trace_eval = true; break;
      case 'c': cont_passing_mode = false; break;
      case 'p': cont_passing_mode = true; break;
      case 'i': immediate_mode = true; break;
      case 'f': immediate_mode = false; break;
      case 'h': show_help(); rexit(0);
      case 'r': to_result_code = true; break;
      case 'o': to_result_code = false; break;
      default:
        std::cerr << "unknown flag '" << *p << "'" << std::endl;
        rexit(-1);
      }
    }
    param_cnt--;
  }
  if (param_cnt != 2) {
    if (param_cnt > 2)
      std::cerr << "too many parameters (did you enclose expression in \"\"?)" << std::endl;
    else
      std::cerr << "expected " << (immediate_mode ? "expression" : "file name") << std::endl;
    rexit(-1);
  }
  string expression;
  if (immediate_mode)
    expression = params[1];
  else
  {
    std::ifstream file(params[1]);
    if (!file) {
      std::cerr << "can't open '" << params[1] << "'" << std::endl;
      rexit(-1);
    }
    expression.assign(
      std::istreambuf_iterator<char>(file),
      std::istreambuf_iterator<char>());
  }
  size_t ctx = reset_global_ctx();
  const char* expr_pos = expression.c_str();
  size_t fn = parse(expr_pos);
  skip_ws(expr_pos);
  if (expr_pos == &error_marker)
      std::cerr << "not matched '(' at " << last_open_par << std::endl;
  else if (*expr_pos)
      std::cerr << "error at " << expr_pos << std::endl;
  else {
    size_t result = (cont_passing_mode ? cont_eval : eval)(fn, ctx);
    if (to_result_code)
      rexit(get_int(result));
    else
      std::cout << format(result) << std::endl;
  }
  return 0;
}
