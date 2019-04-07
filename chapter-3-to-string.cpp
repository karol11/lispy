#include <iostream>
using std::cerr;
using std::endl;

#include <string>
using std::string;

#include <unordered_map>
using std::unordered_map;

#define assert(E) if (!(E)) { cerr << "assert at " << __LINE__ << endl; exit(-1); }

bool trace_gc = false;

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
size_t allocated_count;
size_t first_free;
unordered_map<string, size_t> symbols;

int get_int(size_t v) { return vars[v].h == tNum ? vars[v].v : 0; }
size_t h(size_t v) { return vars[v].h < tVal ? vars[v].h : 0; }
size_t t(size_t v) { return vars[v].h < tVal ? vars[v].t : 0; }

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
  if (first_free) {
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

size_t mk_int(int v) {
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

const size_t kMark = ~(~0u >> 1);

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

int main(int param_cnt, const char* const* params) {
  allocator_test();
  gc_test();
  visualization_test();
  return 0;
}
