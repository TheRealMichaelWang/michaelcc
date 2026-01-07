#ifndef MICHAELCC_UTILS_HPP
#define MICHAELCC_UTILS_HPP

namespace michaelcc {
    template<typename... NodeTypes>
    class generic_visitor;

    template<>
    class generic_visitor<> {
    public:
        virtual ~generic_visitor() = default;
        void visit() const {}
    };

    template<typename NodeType, typename... Rest>
    class generic_visitor<NodeType, Rest...> : public generic_visitor<Rest...> {
    public:
        using generic_visitor<Rest...>::visit;
        virtual void visit(const NodeType& node) {}
    };

    template<typename Visitor>
    class visitable_base {
    public:
        virtual ~visitable_base() = default;
        virtual void accept(Visitor& v) const = 0;
    };
}

#endif
