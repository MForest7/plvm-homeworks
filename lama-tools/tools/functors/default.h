#ifndef FUNCTOR_DEFAULT_H
#define FUNCTOR_DEFAULT_H

template <unsigned char opcode, typename... Args>
struct DefaultFunctor {
    inline void operator()(Args...) {}
};

#endif // FUNCTOR_DEFAULT_H
