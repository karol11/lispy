#include <iostream>
using std::cerr;
using std::endl;

#include <string>
using std::string;

#include <unordered_map>
using std::unordered_map;

#define assert(E) if (!(E)) { cerr << "assert at " << __LINE__ << endl; exit(-1); }

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

int main(int param_cnt, const char* const* params) {
  allocator_test();
  return 0;
}