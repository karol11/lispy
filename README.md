# Lispy. The almost lisp machine in almost 200 lines of C++
I have three reasons to write this thing:
* Lisp is special. It is not the ordinary programming language. Instead it is the mathematical lambda calculus implemented in the form we can run on modern computers. And thus it already contains all the ideas we can find in any programming language until now and in the future. These features include but are not limited to: high order functions, lexical closures, objects and interfaces, co-routines, monads and many more dangerous and scientific looking words. Writing your own lisp machine lets you touch this magic of science.
* If Greenspun's 10th rule is correct, it would be good idea to develop from scratch an ad-hoc informally specified bug ridden slow implementation of lisp in advance to just reuse it in all my ongoing projects.
* Once a life every programmer writes their own programming language, text editor, operation system and lisp machine. It is strange but suddenly I realized that I missed that item in my bucket list.

Lisp machine is an ambiguous term. It's might refer two a full-scale standard implementation of Common Lisp or Scheme or just a small subset of vital built-in functions and macros combined with the a small parser-interpreter loop. All three mentioned reasons guide me to prefer the last variant: not the full standard implementation but an open-ended code open for any sorts of experiments and modifications.

So my goal is: an s-expression parser and a lambda evaluator in 200 lines of C++ code. I will write it in portions adding code system-by-system and describing it in the series of publications.

High-level plan:
* [Data structures and allocator](https://andreykoder.wordpress.com/2019/03/17/lispy1-data-structures/),
* [Garbage collector](https://andreykoder.wordpress.com/2019/03/24/lispy2-garbage-collector/),
* [Visualization (to_string)](https://andreykoder.wordpress.com/2019/04/01/lispy3-visualization/)
* Basic built-in primitives,
* [Parser,](https://andreykoder.wordpress.com/2019/04/07/lispy4-parsing/)
* Interpreter loop,
* Command-line app.
