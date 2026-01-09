#ifndef MICHAELCC_TYPING_HPP
#define MICHAELCC_TYPING_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include "utils.hpp"

namespace michaelcc {
    namespace typing {
		// Storage class specifiers (for declarations)
		enum storage_class {
			NO_STORAGE_CLASS = 0,
			EXTERN_STORAGE_CLASS = 1,
			STATIC_STORAGE_CLASS = 2,
			REGISTER_STORAGE_CLASS = 4,
			AUTO_STORAGE_CLASS = 8
		};

		// Type qualifiers (for qualified_type wrapper)
		enum type_qualifier {
			NO_TYPE_QUALIFIER = 0,
			CONST_TYPE_QUALIFIER = 1,
			VOLATILE_TYPE_QUALIFIER = 2,
			RESTRICT_TYPE_QUALIFIER = 4
		};

        enum int_qualifier {
			NO_INT_QUALIFIER = 0,
			LONG_INT_QUALIFIER = 1,
			SIGNED_INT_QUALIFIER = 4,
			UNSIGNED_INT_QUALIFIER = 8
		};

		enum int_class {
			CHAR_INT_CLASS,
			SHORT_INT_CLASS,
			INT_INT_CLASS,
			LONG_INT_CLASS
		};

		enum float_class {
			FLOAT_FLOAT_CLASS,
			DOUBLE_FLOAT_CLASS
		};

        class base_type;
        class void_type;
        class int_type;
        class float_type;
        class pointer_type;
        class array_type;
        class enum_type;
        class function_pointer_type;
        class struct_type;
        class union_type;

        template<typename ReturnType>
        class type_dispatcher : public generic_dispatcher<ReturnType, base_type, 
            void_type, 
            int_type, 
            float_type, 
            pointer_type, 
            array_type, 
            enum_type, 
            function_pointer_type,
            struct_type,
            union_type> {};
        
            class base_type {
        public:
            virtual ~base_type() = default;

            virtual bool is_assignable_from(const base_type& other) const = 0;

            virtual void to_string(std::ostringstream& ss) const = 0;
        };

        template<typename Ptr>
        struct base_type_ptr_access {
            static std::shared_ptr<base_type> get(const Ptr& p) { return p; }
        };

        template<>
        struct base_type_ptr_access<std::weak_ptr<base_type>> {
            static std::shared_ptr<base_type> get(const std::weak_ptr<base_type>& p) { return p.lock(); }
        };

        template<typename Ptr = std::shared_ptr<base_type>>
        class basic_qualified_type {
        private:
            Ptr m_type;
            uint8_t m_qualifiers;

        public:
            basic_qualified_type() : m_type(), m_qualifiers(NO_TYPE_QUALIFIER) {}
            
            basic_qualified_type(Ptr type, uint8_t qualifiers = NO_TYPE_QUALIFIER)
                : m_type(std::move(type)), m_qualifiers(qualifiers) {}

            template<typename Ptr2>
            bool operator == (const basic_qualified_type<Ptr2>& other) const {
                auto base_type1 = base_type_ptr_access<Ptr>::get(m_type);
                auto base_type2 = base_type_ptr_access<Ptr2>::get(other.type_ptr());
                return base_type1->is_assignable_from(*base_type2) && base_type2->is_assignable_from(*base_type1)
                    && m_qualifiers == other.qualifiers();
            }

            std::shared_ptr<base_type> type() const noexcept { 
                return base_type_ptr_access<Ptr>::get(m_type); 
            }

            const Ptr& type_ptr() const noexcept { return m_type; }

            uint8_t qualifiers() const noexcept { return m_qualifiers; }
            bool is_const() const noexcept { return m_qualifiers & CONST_TYPE_QUALIFIER; }
            bool is_volatile() const noexcept { return m_qualifiers & VOLATILE_TYPE_QUALIFIER; }
            bool is_restrict() const noexcept { return m_qualifiers & RESTRICT_TYPE_QUALIFIER; }

            basic_qualified_type<std::weak_ptr<base_type>> to_weak() const {
                return basic_qualified_type<std::weak_ptr<base_type>>(
                    std::weak_ptr<base_type>(base_type_ptr_access<Ptr>::get(m_type)), 
                    m_qualifiers
                );
            }

            basic_qualified_type<std::shared_ptr<base_type>> to_shared() const {
                return basic_qualified_type<std::shared_ptr<base_type>>(
                    base_type_ptr_access<Ptr>::get(m_type), 
                    m_qualifiers
                );
            }

            void to_string(std::ostringstream& ss) const {
                if (is_const()) ss << "const ";
                if (is_volatile()) ss << "volatile ";
                if (is_restrict()) ss << "restrict ";
                type()->to_string(ss);
            }

            std::string to_string() const {
                std::ostringstream ss;
                to_string(ss);
                return ss.str();
            }
        };

        using qual_type = basic_qualified_type<std::shared_ptr<base_type>>;
        using weak_qual_type = basic_qualified_type<std::weak_ptr<base_type>>;

        class void_type : public base_type {
        public:
            void_type() = default;

            bool is_assignable_from(const base_type& other) const override {
                return typeid(other) == typeid(*this);
            }

            void to_string(std::ostringstream& ss) const override {
                ss << "void";
            }
        };

        class int_type : public base_type {
        private:
            uint8_t m_int_qualifiers;
            int_class m_class;

        public:
            int_type(uint8_t int_qualifiers, int_class type_class)
                : m_int_qualifiers(int_qualifiers), m_class(type_class) {}

            int_class type_class() const noexcept { return m_class; }
            uint8_t int_qualifiers() const noexcept { return m_int_qualifiers; }

            bool is_unsigned() const noexcept { return m_int_qualifiers & UNSIGNED_INT_QUALIFIER; }
            bool is_signed() const noexcept { return m_int_qualifiers & SIGNED_INT_QUALIFIER; }
            bool is_long() const noexcept { return m_int_qualifiers & LONG_INT_QUALIFIER; }

            bool is_assignable_from(const base_type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const int_type& other_int = static_cast<const int_type&>(other);
                return m_int_qualifiers == other_int.int_qualifiers() 
                    && m_class == other_int.type_class();
            }

            void to_string(std::ostringstream& ss) const override {
                if (is_unsigned()) ss << "unsigned ";
                else if (is_signed()) ss << "signed ";
                if (is_long()) ss << "long ";
                switch (m_class) {
                    case CHAR_INT_CLASS: ss << "char"; break;
                    case SHORT_INT_CLASS: ss << "short"; break;
                    case INT_INT_CLASS: ss << "int"; break;
                    case LONG_INT_CLASS: ss << "long long"; break;
                }
            }
        };

        class float_type : public base_type {
        private:
            float_class m_class;

        public:
            explicit float_type(float_class type_class)
                : m_class(type_class) {}

            float_class type_class() const noexcept { return m_class; }

            bool is_assignable_from(const base_type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const float_type& other_float = static_cast<const float_type&>(other);
                return m_class == other_float.type_class();
            }

            void to_string(std::ostringstream& ss) const override {
                ss << (m_class == FLOAT_FLOAT_CLASS ? "float" : "double");
            }
        };

        class array_type : public base_type {
        private:
            weak_qual_type m_element_type;

        public:
            array_type(weak_qual_type element_type)
                : m_element_type(std::move(element_type)) {}

            const weak_qual_type& element_type() const noexcept { return m_element_type; }

            bool is_assignable_from(const base_type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const array_type& other_arr = static_cast<const array_type&>(other);
                return m_element_type == other_arr.element_type();
            }

            void to_string(std::ostringstream& ss) const override {
                m_element_type.to_string(ss);
                ss << "[]";
            }
        };

        class function_pointer_type : public base_type {
        private:
            weak_qual_type m_return_type;
            std::vector<weak_qual_type> m_parameter_types;

        public:
            function_pointer_type(weak_qual_type return_type, std::vector<weak_qual_type> parameter_types)
                : m_return_type(std::move(return_type)), m_parameter_types(std::move(parameter_types)) {}

            const weak_qual_type& return_type() const noexcept { return m_return_type; }
            const std::vector<weak_qual_type>& parameter_types() const noexcept { return m_parameter_types; }

            bool is_assignable_from(const base_type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const function_pointer_type& other_fptr = static_cast<const function_pointer_type&>(other);
                if (m_parameter_types.size() != other_fptr.parameter_types().size()) {
                    return false;
                }
                for (size_t i = 0; i < m_parameter_types.size(); i++) {
                    // Parameters are contravariant
                    if (!other_fptr.parameter_types()[i].type()->is_assignable_from(*m_parameter_types[i].type())) {
                        return false;
                    }
                }

                return m_return_type.type()->is_assignable_from(*other_fptr.return_type().type());
            }

            void to_string(std::ostringstream& ss) const override {
                m_return_type.to_string(ss);
                ss << " (*)(";
                for (size_t i = 0; i < m_parameter_types.size(); i++) {
                    if (i > 0) ss << ", ";
                    m_parameter_types[i].to_string(ss);
                }
                ss << ")";
            }
        };

        class pointer_type : public base_type {
        private:
            weak_qual_type m_pointee_type;

        public:
            pointer_type(weak_qual_type pointee_type)
                : m_pointee_type(std::move(pointee_type)) {}

            const weak_qual_type& pointee_type() const noexcept { return m_pointee_type; }

            bool is_assignable_from(const base_type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    // void* can accept function pointers
                    if (typeid(other) == typeid(function_pointer_type)
                        && dynamic_cast<const void_type*>(m_pointee_type.type().get()) != nullptr) {
                        return true;
                    }
                    return false;
                }

                const pointer_type& other_ptr = static_cast<const pointer_type&>(other);
                
                // void* accepts any pointer
                if (dynamic_cast<const void_type*>(m_pointee_type.type().get()) != nullptr) {
                    return true;
                }

                return m_pointee_type.type()->is_assignable_from(*other_ptr.pointee_type().type());
            }

            void to_string(std::ostringstream& ss) const override {
                m_pointee_type.to_string(ss);
                ss << "*";
            }
        };

        class struct_type final : public base_type {
        public:
            struct field {
                std::string name;
                weak_qual_type field_type;
                size_t offset = 0;

                field(std::string n, weak_qual_type t) 
                    : name(std::move(n)), field_type(std::move(t)) {}
            };

        private:
            std::optional<std::string> m_name;
            std::vector<field> m_fields;
        
            friend class type_context;

        public:
            struct_type(std::optional<std::string> name, std::vector<field> fields)
                : m_name(std::move(name)), m_fields(std::move(fields)) {}

            const std::optional<std::string>& name() const noexcept { return m_name; }
            const std::vector<field>& fields() const noexcept { return m_fields; }

            bool is_assignable_from(const base_type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const struct_type& other_struct = static_cast<const struct_type&>(other);

                // Named structs: same name = same type
                if (m_name.has_value() && other_struct.name().has_value()) {
                    return m_name == other_struct.name();
                }

                // Anonymous structs: structural comparison
                if (m_fields.size() != other_struct.fields().size()) {
                    return false;
                }

                for (size_t i = 0; i < m_fields.size(); i++) {
                    if (m_fields[i].field_type != other_struct.fields()[i].field_type) {
                        return false;
                    }
                }

                return true;
            }

            bool is_implemented() const noexcept { 
                for (const auto& field : m_fields) {
                    if (field.field_type.type()->is_assignable_from(void_type())) {
                        return false;
                    }
                }
                return true;
            }

            bool implement_field_types(std::vector<weak_qual_type>&& field_types) {
                if (field_types.size() != m_fields.size()) {
                    return false;
                }
                for (size_t i = 0; i < m_fields.size(); i++) {
                    m_fields[i].field_type = std::move(field_types[i]);
                }
                return true;
            }

            void implement_field_offsets(const std::vector<size_t>& field_offsets) {
                for (size_t i = 0; i < m_fields.size(); i++) {
                    m_fields[i].offset = field_offsets[i];
                }
            }

            void to_string(std::ostringstream& ss) const override {
                if (m_name.has_value()) {
                    ss << "struct " << m_name.value();
                    return;
                }
                ss << "struct { ";
                for (const auto& f : m_fields) {
                    f.field_type.to_string(ss);
                    ss << " " << f.name << "; ";
                }
                ss << "}";
            }
        };

        class union_type final : public base_type {
        public:
            struct member {
                std::string name;
                weak_qual_type member_type;

                member(std::string n, weak_qual_type t)
                    : name(std::move(n)), member_type(std::move(t)) {}
            };

        private:
            std::optional<std::string> m_name;
            std::vector<member> m_members;

            friend class type_context;

        public:
            union_type(std::optional<std::string> name, std::vector<member> members)
                : m_name(std::move(name)), m_members(std::move(members)) {}

            const std::optional<std::string>& name() const noexcept { return m_name; }
            const std::vector<member>& members() const noexcept { return m_members; }

            bool is_assignable_from(const base_type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const union_type& other_union = static_cast<const union_type&>(other);
                
                // Named unions: same name = same type
                if (m_name.has_value() && other_union.name().has_value()) {
                    return m_name == other_union.name();
                }

                // Anonymous unions: structural comparison
                if (m_members.size() != other_union.members().size()) {
                    return false;
                }

                std::map<std::string, weak_qual_type> member_types;
                for (const auto& m : m_members) {
                    member_types[m.name] = m.member_type;
                }
                for (const auto& m : other_union.members()) {
                    auto it = member_types.find(m.name);
                    if (it == member_types.end() || it->second != m.member_type) {
                        return false;
                    }
                }

                return true;
            }

            bool is_implemented() const noexcept { 
                for (const auto& member : m_members) {
                    if (member.member_type.type()->is_assignable_from(void_type())) {
                        return false;
                    }
                }
                return true;
            }

            bool implement_member_types(std::vector<weak_qual_type>&& member_types) {
                if (member_types.size() != m_members.size()) {
                    return false;
                }
                for (size_t i = 0; i < m_members.size(); i++) {
                    m_members[i].member_type = std::move(member_types[i]);
                }
                return true;
            }

            void to_string(std::ostringstream& ss) const override {
                if (m_name.has_value()) {
                    ss << "union " << m_name.value();
                    return;
                }
                ss << "union { ";
                for (const auto& m : m_members) {
                    m.member_type.to_string(ss);
                    ss << " " << m.name << "; ";
                }
                ss << "}";
            }
        };

        class enum_type final : public base_type {
        public:
            struct enumerator {
                std::string name;
                int64_t value;
            };

        private:
            std::optional<std::string> m_name;
            std::vector<enumerator> m_enumerators;

        public:
            enum_type(std::optional<std::string> name, std::vector<enumerator> enumerators)
                : m_name(std::move(name)), m_enumerators(std::move(enumerators)) {}

            const std::optional<std::string>& name() const noexcept { return m_name; }
            const std::vector<enumerator>& enumerators() const noexcept { return m_enumerators; }

            bool is_assignable_from(const base_type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const enum_type& other_enum = static_cast<const enum_type&>(other);
                
                // Named enums: same name = same type
                if (m_name.has_value() && other_enum.name().has_value()) {
                    return m_name == other_enum.name();
                }

                // Anonymous enums: structural comparison
                if (m_enumerators.size() != other_enum.enumerators().size()) {
                    return false;
                }
                for (size_t i = 0; i < m_enumerators.size(); i++) {
                    if (m_enumerators[i].value != other_enum.enumerators()[i].value) {
                        return false;
                    }
                }

                return true;
            }

            void to_string(std::ostringstream& ss) const override {
                if (m_name.has_value()) {
                    ss << "enum " << m_name.value();
                    return;
                }
                ss << "enum { ";
                for (size_t i = 0; i < m_enumerators.size(); i++) {
                    if (i > 0) ss << ", ";
                    ss << m_enumerators[i].name << " = " << m_enumerators[i].value;
                }
                ss << " }";
            }
        };

        // Static singleton instances of primitive types
        struct primitives {
            // void
            inline static const std::shared_ptr<void_type> void_t = std::make_shared<void_type>();

            // char variants
            inline static const std::shared_ptr<int_type> char_t = 
                std::make_shared<int_type>(NO_INT_QUALIFIER, CHAR_INT_CLASS);
            inline static const std::shared_ptr<int_type> signed_char_t = 
                std::make_shared<int_type>(SIGNED_INT_QUALIFIER, CHAR_INT_CLASS);
            inline static const std::shared_ptr<int_type> unsigned_char_t = 
                std::make_shared<int_type>(UNSIGNED_INT_QUALIFIER, CHAR_INT_CLASS);

            // short variants
            inline static const std::shared_ptr<int_type> short_t = 
                std::make_shared<int_type>(NO_INT_QUALIFIER, SHORT_INT_CLASS);
            inline static const std::shared_ptr<int_type> unsigned_short_t = 
                std::make_shared<int_type>(UNSIGNED_INT_QUALIFIER, SHORT_INT_CLASS);

            // int variants
            inline static const std::shared_ptr<int_type> int_t = 
                std::make_shared<int_type>(NO_INT_QUALIFIER, INT_INT_CLASS);
            inline static const std::shared_ptr<int_type> unsigned_int_t = 
                std::make_shared<int_type>(UNSIGNED_INT_QUALIFIER, INT_INT_CLASS);

            // long variants
            inline static const std::shared_ptr<int_type> long_t = 
                std::make_shared<int_type>(LONG_INT_QUALIFIER, INT_INT_CLASS);
            inline static const std::shared_ptr<int_type> unsigned_long_t = 
                std::make_shared<int_type>(LONG_INT_QUALIFIER | UNSIGNED_INT_QUALIFIER, INT_INT_CLASS);

            // long long variants
            inline static const std::shared_ptr<int_type> long_long_t = 
                std::make_shared<int_type>(NO_INT_QUALIFIER, LONG_INT_CLASS);
            inline static const std::shared_ptr<int_type> unsigned_long_long_t = 
                std::make_shared<int_type>(UNSIGNED_INT_QUALIFIER, LONG_INT_CLASS);

            // float variants
            inline static const std::shared_ptr<float_type> float_t = 
                std::make_shared<float_type>(FLOAT_FLOAT_CLASS);
            inline static const std::shared_ptr<float_type> double_t = 
                std::make_shared<float_type>(DOUBLE_FLOAT_CLASS);
        };
    }
}
#endif
