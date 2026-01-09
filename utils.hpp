#ifndef MICHAELCC_UTILS_HPP
#define MICHAELCC_UTILS_HPP

#include <type_traits>

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

    template<typename ReturnType, typename... NodeTypes>
    class generic_dispatcher;

    template<typename ReturnType>
    class generic_dispatcher<ReturnType> {
    public:
        virtual ~generic_dispatcher() = default;

        template<typename BaseType>
        ReturnType dispatch_dynamic(const BaseType&) const {
            return ReturnType{};
        }

    protected:
        // Sentinel for the using declaration chain - never called
        void dispatch() const {}
    };

    template<typename ReturnType, typename NodeType, typename... Rest>
    class generic_dispatcher<ReturnType, NodeType, Rest...> : public generic_dispatcher<ReturnType, Rest...> {
    protected:
        using generic_dispatcher<ReturnType, Rest...>::dispatch;
        virtual ReturnType dispatch(const NodeType& node) const = 0;

    public:
        template<typename BaseType>
        ReturnType dispatch_dynamic(const BaseType& node) const {
            static_assert(std::is_base_of_v<BaseType, NodeType>, 
                "BaseType must be a base class of NodeType");
            if (auto* p = dynamic_cast<const NodeType*>(&node)) {
                return dispatch(*p);
            }
            return generic_dispatcher<ReturnType, Rest...>::dispatch_dynamic(node);
        }
    };
}

#endif
