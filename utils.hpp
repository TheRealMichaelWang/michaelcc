#ifndef MICHAELCC_UTILS_HPP
#define MICHAELCC_UTILS_HPP

#include <tuple>

namespace michaelcc {
    template<typename ChildType, auto Accessor>
    struct child {
        using type = ChildType;
        
        template<typename Node>
        static const ChildType* get(const Node& node) {
            return (node.*Accessor)();
        }
    };

    template<typename Node>
    struct node_info {
        using children = std::tuple<>;
    };

    template<typename Node, typename ReturnType>
    struct children_tuple {
        using type = decltype(std::apply([](auto... infos) {
            return std::tuple<decltype(ReturnType{}, infos, ReturnType{})...>{};
        }, typename node_info<Node>::children{}));
    };

    template<typename Node, typename ReturnType>
    using children_tuple_t = typename children_tuple<Node, ReturnType>::type;

    template<typename Derived, typename Visitor, typename Base>
    class visitable_base : public Base {
    public:
        using Base::Base;
        
        void accept(Visitor& v) const override {
            const auto& self = static_cast<const Derived&>(*this);
            v.visit(self);
            std::apply([&](auto... child_infos) {
                auto visit_one = [&](auto info) {
                    if (auto* c = decltype(info)::get(self)) {
                        c->accept(v);
                    }
                };
                (visit_one(child_infos), ...);
            }, typename node_info<Derived>::children{});
        }
    };

    template<typename Derived, typename Transformer, typename ReturnType>
    class transformable_base {
    public:
        virtual ~transformable_base() = default;

        ReturnType accept_transform(Transformer& t) const {
            const auto& self = static_cast<const Derived&>(*this);
            auto children = std::apply([&](auto... child_infos) {
                auto transform_one = [&](auto info) -> ReturnType {
                    auto* c = decltype(info)::get(self);
                    return c ? c->accept_transform(t) : ReturnType{};
                };
                return std::make_tuple(transform_one(child_infos)...);
            }, typename node_info<Derived>::children{});
            return t.transform(self, std::move(children));
        }
    };

    template<typename... NodeTypes>
    class auto_visitor;

    template<>
    class auto_visitor<> {
    public:
        virtual ~auto_visitor() = default;
        void visit() {}
    };

    template<typename NodeType, typename... Rest>
    class auto_visitor<NodeType, Rest...> : public auto_visitor<Rest...> {
    public:
        using auto_visitor<Rest...>::visit;
        virtual void visit(const NodeType& node) {}
    };

    // Transformer: generates transform(const NodeType&, children_tuple) methods
    template<typename ReturnType, typename... NodeTypes>
    class auto_transformer;

    template<typename ReturnType>
    class auto_transformer<ReturnType> {
    public:
        virtual ~auto_transformer() = default;
        ReturnType transform() { return ReturnType{}; }
    };

    template<typename ReturnType, typename NodeType, typename... Rest>
    class auto_transformer<ReturnType, NodeType, Rest...> 
        : public auto_transformer<ReturnType, Rest...> {
    public:
        using auto_transformer<ReturnType, Rest...>::transform;
        using children_t = children_tuple_t<NodeType, ReturnType>;
        
        virtual ReturnType transform(const NodeType& node, children_t children) = 0;
    };
}

// Macro to declare a child accessor
// Usage: CHILD(ast_element, left) expands to child<ast_element, &_NodeType::left>
#define MICHAELCC_AST_CHILD(ChildType, accessor) \
    michaelcc::child<ChildType, &_NodeType::accessor>

// Macro to specialize node_info for a node type
// Usage: NODE_INFO(ast::arithmetic_operator, CHILD(ast_element, left), CHILD(ast_element, right))
#define MICHAELCC_AST_NODE_INFO(NodeType, ...) \
    template<> \
    struct michaelcc::node_info<NodeType> { \
        using _NodeType = NodeType; \
        using children = std::tuple<__VA_ARGS__>; \
    }

// Macro for nodes with no children
#define MICHAELCC_AST_NODE_INFO_LEAF(NodeType) \
    template<> \
    struct michaelcc::node_info<NodeType> { \
        using children = std::tuple<>; \
    }

#endif
