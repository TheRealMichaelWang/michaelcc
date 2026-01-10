#ifndef MICHAELCC_UTILS_HPP
#define MICHAELCC_UTILS_HPP

#include <concepts>
#include <stdexcept>
#include <typeinfo>

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

    template<typename ReturnType, typename BaseType, typename... NodeTypes>
    class generic_dispatcher;

    template<typename ReturnType, typename BaseType>
    class generic_dispatcher<ReturnType, BaseType> {
    public:
        virtual ~generic_dispatcher() = default;

        ReturnType operator()(BaseType&) {
            throw std::runtime_error("No dispatch method for type " + std::string(typeid(BaseType).name()));
        }

    protected:
        // Sentinel for the using declaration chain - never called
        void dispatch() {}
    };

    template<typename ReturnType, typename BaseType, typename NodeType, typename... Rest>
    requires std::derived_from<NodeType, BaseType>
    class generic_dispatcher<ReturnType, BaseType, NodeType, Rest...> : public generic_dispatcher<ReturnType, BaseType, Rest...> {
    protected:

        using generic_dispatcher<ReturnType, BaseType, Rest...>::dispatch;
        virtual ReturnType dispatch(NodeType& node) = 0;

    public:
        ReturnType operator()(BaseType& node) {
            if (auto* p = dynamic_cast<const NodeType*>(&node)) {
                return dispatch(*p);
            }
            return generic_dispatcher<ReturnType, BaseType, Rest...>::operator()(node);
        }
    };
}

#endif
