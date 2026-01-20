#ifndef MICHAELCC_UTILS_HPP
#define MICHAELCC_UTILS_HPP

#include <concepts>
#include <stdexcept>
#include <typeinfo>
#include <memory>
#include <format>

template <typename Derived, typename Base>
std::unique_ptr<Derived> dynamic_unique_cast(std::unique_ptr<Base>&& base) {
    if (Derived* d = dynamic_cast<Derived*>(base.get())) {
        base.release(); // Relinquish ownership from base
        return std::unique_ptr<Derived>(d); // Take ownership in new type
    }
    return nullptr; // Cast failed; base still owns the original object
}
namespace michaelcc {
    template<typename... NodeTypes>
    class const_generic_visitor;

    template<>
    class const_generic_visitor<> {
    public:
        virtual ~const_generic_visitor() = default;
        void visit() const {}
    };

    template<typename NodeType, typename... Rest>
    class const_generic_visitor<NodeType, Rest...> : public const_generic_visitor<Rest...> {
    public:
        using const_generic_visitor<Rest...>::visit;
        virtual void visit(const NodeType& node) {}
    };

    template<typename... NodeTypes>
    class mutable_generic_visitor;

    template<>
    class mutable_generic_visitor<> {
    public:
        virtual ~mutable_generic_visitor() = default;
        void visit() {}
    };

    template<typename NodeType, typename... Rest>
    class mutable_generic_visitor<NodeType, Rest...> : public mutable_generic_visitor<Rest...> {
    public:
        using mutable_generic_visitor<Rest...>::visit;
        virtual void visit(NodeType& node) {}
    };


    template<typename Visitor>
    class const_visitable_base {
    public:
        virtual ~const_visitable_base() = default;
        virtual void accept(Visitor& v) const = 0;
    };

    template<typename Visitor>
    class mutable_visitable_base {
    public:
        virtual ~mutable_visitable_base() = default;
        virtual void mutable_accept(Visitor& v) = 0;
    };

    template<typename ReturnType, typename BaseType, typename... NodeTypes>
    class generic_dispatcher;

    template<typename ReturnType, typename BaseType>
    class generic_dispatcher<ReturnType, BaseType> {
    public:
        virtual ~generic_dispatcher() = default;

        ReturnType operator()(BaseType& node) {
            handle_default(node);
            throw std::runtime_error("handle_default must throw");
        }

    protected:
        virtual void handle_default(const BaseType& node) {
            throw std::runtime_error(std::format("No dispatch method for type {}", typeid(BaseType).name()));
        }
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
            if (auto* p = dynamic_cast<NodeType*>(&node)) {
                return dispatch(*p);
            }
            return generic_dispatcher<ReturnType, BaseType, Rest...>::operator()(node);
        }
    };

    template<typename BaseType, typename... NodeTypes>
    class generic_transformer;

    template<typename BaseType>
    class generic_transformer<BaseType> {
    public:
        virtual ~generic_transformer() = default;

        std::unique_ptr<BaseType> operator()(std::unique_ptr<BaseType>&& node) {
            handle_default(node);
            return nullptr;
        }

    protected:
        // Sentinel for the using declaration chain - never called
        void dispatch() {}
    };

    template<typename BaseType, typename NodeType, typename... Rest>
    requires std::derived_from<NodeType, BaseType>
    class generic_transformer<BaseType, NodeType, Rest...> : public generic_transformer<BaseType, Rest...> {
    protected:

        using generic_transformer<BaseType, Rest...>::dispatch;
        virtual std::unique_ptr<BaseType> dispatch(std::unique_ptr<NodeType>&& node) = 0;

    public:
        std::unique_ptr<BaseType> operator()(std::unique_ptr<BaseType>&& node) {
            std::unique_ptr<NodeType> n = dynamic_unique_cast<NodeType>(std::move(node));
            if (n) {
                return dispatch(std::move(n));
            }
            return generic_transformer<BaseType, Rest...>::operator()(std::move(node));
        }
    };

    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
}



#endif
