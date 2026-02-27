#ifndef MICHAELCC_TYPE_INFO_HPP
#define MICHAELCC_TYPE_INFO_HPP

#include "linear/registers.hpp"
#include "logic/typing.hpp"
#include "platform.hpp"
#include <map>

namespace michaelcc {
    struct type_layout_info {
        const size_t size;
        const size_t alignment;

        static linear::word_size get_register_size(size_t size_bytes);
    };

    class type_layout_calculator final : public typing::type_dispatcher<const type_layout_info> {
    private:
        std::map<const typing::base_type*, type_layout_info> m_declared_info;
        platform_info m_platform_info;

        const type_layout_info pointer_layout = {
            .size=static_cast<size_t>(m_platform_info.pointer_size) / 8,
            .alignment=std::min<size_t>(static_cast<size_t>(m_platform_info.pointer_size) / 8, m_platform_info.max_alignment)
        };

        const type_layout_info int_layout = {
            .size=static_cast<size_t>(m_platform_info.int_size) / 8,
            .alignment=std::min<size_t>(static_cast<size_t>(m_platform_info.int_size) / 8, m_platform_info.max_alignment)
        };


    public:
        type_layout_calculator(const platform_info& platform_info) : m_platform_info(platform_info) {}

        const platform_info& get_platform_info() const noexcept { return m_platform_info; }

        bool must_alloca(const typing::qual_type type) noexcept;

        // Static method to get int type size
        static linear::word_size get_int_type_size(const typing::int_type& type, const platform_info& platform);

    protected:
        const type_layout_info dispatch(typing::void_type& type) override {
            throw std::runtime_error("Void type is not a valid type for layout calculation");
        }

        const type_layout_info dispatch(typing::int_type& type) override;

        const type_layout_info dispatch(typing::float_type& type) override;

        const type_layout_info dispatch(typing::pointer_type& type) override { return pointer_layout; }

        const type_layout_info dispatch(typing::array_type& type) override { return pointer_layout; }

        const type_layout_info dispatch(typing::enum_type& type) override { return int_layout; }

        const type_layout_info dispatch(typing::function_pointer_type& type) override { return pointer_layout; }

        const type_layout_info dispatch(typing::struct_type& type) override;
        const type_layout_info dispatch(typing::union_type& type) override;
    };
}

#endif
