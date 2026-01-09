#ifndef MICHAELCC_TYPING_HPP
#define MICHAELCC_TYPING_HPP

#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <typeinfo>
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

        class type;
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
        class type_dispatcher : public generic_dispatcher<ReturnType, type, 
            void_type, 
            int_type, 
            float_type, 
            pointer_type, 
            array_type, 
            enum_type, 
            function_pointer_type,
            struct_type,
            union_type> {};

        class type {
        public:
            virtual ~type() = default;

            virtual bool is_assignable_from(const type& other) const = 0;

            virtual std::string to_string() const = 0;
        };

        inline bool are_quivalent(const type& lhs, const type& rhs) {
            return lhs.is_assignable_from(rhs) && rhs.is_assignable_from(lhs);
        }

        class void_type : public type {
        public:
            bool is_assignable_from(const type& other) const override {
                return typeid(other) == typeid(*this);
            }

            std::string to_string() const override {
                return "void";
            }
        };

        class int_type : public type {
        private:
            uint8_t m_qualifiers;
            int_class m_class;
        public:
            int_type(uint8_t qualifiers, int_class m_class)
                : m_qualifiers(qualifiers), m_class(m_class) { }

            uint8_t qualifiers() const noexcept { return m_qualifiers; }
            int_class type_class() const noexcept { return m_class; }

            bool is_assignable_from(const type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const int_type& other_int = static_cast<const int_type&>(other);
                return m_qualifiers == other_int.qualifiers() && m_class == other_int.type_class();
            }

            std::string to_string() const override {
                std::ostringstream ss;
                if (m_qualifiers & UNSIGNED_INT_QUALIFIER) ss << "unsigned ";
                else if (m_qualifiers & SIGNED_INT_QUALIFIER) ss << "signed ";
                if (m_qualifiers & LONG_INT_QUALIFIER) ss << "long ";
                switch (m_class) {
                    case CHAR_INT_CLASS: ss << "char"; break;
                    case SHORT_INT_CLASS: ss << "short"; break;
                    case INT_INT_CLASS: ss << "int"; break;
                    case LONG_INT_CLASS: ss << "long long"; break;
                }
                return ss.str();
            }
        };

        class float_type : public type {
        private:
            float_class m_class;

        public:
            explicit float_type(float_class m_class)
                : m_class(m_class) { }

            float_class type_class() const noexcept { return m_class; }

            bool is_assignable_from(const type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const float_type& other_float = static_cast<const float_type&>(other);
                return m_class == other_float.type_class();
            }

            std::string to_string() const override {
                return m_class == FLOAT_FLOAT_CLASS ? "float" : "double";
            }
        };

        class array_type : public type {
        private:
            std::weak_ptr<type> m_element_type;

        public:
            array_type(std::weak_ptr<type>&& element_type)
                : m_element_type(std::move(element_type)) {}

            const std::shared_ptr<type> element_type() const noexcept { return m_element_type.lock(); }

            bool is_assignable_from(const type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const array_type& other_arr = static_cast<const array_type&>(other);
                return are_quivalent(*m_element_type.lock(), *other_arr.element_type());
            }

            std::string to_string() const override {
                std::ostringstream ss;
                ss << m_element_type.lock()->to_string() << "[]";
                return ss.str();
            }
        };

        class function_pointer_type : public type {
        private:
            std::weak_ptr<type> m_return_type;
            std::vector<std::weak_ptr<type>> m_parameter_types;
        public:
            function_pointer_type(std::weak_ptr<type>&& return_type, std::vector<std::weak_ptr<type>>&& parameter_types)
                : m_return_type(std::move(return_type)), m_parameter_types(std::move(parameter_types)) {}

            const std::shared_ptr<type> return_type() const noexcept { return m_return_type.lock(); }
            
            const std::vector<std::shared_ptr<type>> parameter_types() const noexcept { 
                return std::vector<std::shared_ptr<type>>(m_parameter_types.begin(), m_parameter_types.end()); 
            }

            bool is_assignable_from(const type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const function_pointer_type& other_fptr = static_cast<const function_pointer_type&>(other);
                if (m_parameter_types.size() != other_fptr.parameter_types().size()) {
                    return false;
                }
                for (size_t i = 0; i < m_parameter_types.size(); i++) {
                    if (!other_fptr.parameter_types()[i]->is_assignable_from(*m_parameter_types[i].lock())) {
                        return false;
                    }
                }

                return return_type()->is_assignable_from(*other_fptr.return_type());
            }

            std::string to_string() const override {
                std::ostringstream ss;
                ss << m_return_type.lock()->to_string() << " (*)(";
                for (size_t i = 0; i < m_parameter_types.size(); i++) {
                    if (i > 0) ss << ", ";
                    ss << m_parameter_types[i].lock()->to_string();
                }
                ss << ")";
                return ss.str();
            }
        };

        class pointer_type : public type {
        private:
            std::weak_ptr<type> m_pointee_type;

        public:
            pointer_type(std::weak_ptr<type>&& pointee_type)
                : m_pointee_type(std::move(pointee_type)) { }

            const std::shared_ptr<type> pointee_type() const noexcept { return m_pointee_type.lock(); }

            bool is_assignable_from(const type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    if (typeid(other) == typeid(function_pointer_type)
                        && dynamic_cast<const void_type*>(pointee_type().get()) != nullptr) {
                        return true;
                    }

                    return false;
                }

                const pointer_type& other_ptr = dynamic_cast<const pointer_type&>(other);
                if (dynamic_cast<const void_type*>(pointee_type().get()) != nullptr) {
                    return true;
                }

                return m_pointee_type.lock()->is_assignable_from(*other_ptr.pointee_type());
            }

            std::string to_string() const override {
                std::ostringstream ss;
                ss << m_pointee_type.lock()->to_string() << "*";
                return ss.str();
            }
        };

        class struct_type final : public type {
        public:
            struct field {
                std::string name;
                std::weak_ptr<type> field_type = std::weak_ptr<type>();

                size_t offset;
            };
        private:
            std::optional<std::string> m_name;
            std::vector<field> m_fields;
        
            friend class type_context;
        public:
            struct_type(std::optional<std::string>&& name, std::vector<field>&& fields)
                : m_name(std::move(name)), m_fields(std::move(fields)) {}

            const std::optional<std::string>& name() const noexcept { return m_name; }
            const std::vector<field>& fields() const noexcept { return m_fields; }

            bool is_assignable_from(const type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const struct_type& other_struct = static_cast<const struct_type&>(other);

                if (m_name != other_struct.name()) {
                    return false;
                }

                if (fields().size() != other_struct.fields().size()) {
                    return false;
                }

                for (size_t i = 0; i < fields().size(); i++) {
                    if (!fields()[i].field_type.lock()->is_assignable_from(*other_struct.fields()[i].field_type.lock())) {
                        return false;
                    }
                }

                return true;
            }

            bool implemented() const noexcept { 
                for (const auto& field : fields()) {
                    if (field.field_type.lock()) {
                        return false;
                    }
                }
                return true;
            }

            bool implement_field_types(std::vector<std::shared_ptr<type>>&& field_types) {
                if (implemented() || field_types.size() != m_fields.size()) {
                    return false;
                }

                for (size_t i = 0; i < m_fields.size(); i++) {
                    m_fields[i].field_type = std::weak_ptr<type>(field_types[i]);
                }
                return true;
            }

            void implement_field_offsets(const std::vector<size_t>& field_offsets) {
                for (size_t i = 0; i < m_fields.size(); i++) {
                    m_fields[i].offset = field_offsets[i];
                }
            }

            std::string to_string() const override {
                if (m_name.has_value()) {
                    return "struct " + m_name.value();
                }
                std::ostringstream ss;
                ss << "struct { ";
                for (const auto& f : m_fields) {
                    ss << f.field_type.lock()->to_string() << " " << f.name << "; ";
                }
                ss << "}";
                return ss.str();
            }
        };

        class union_type final : public type {
        public:
            struct member {
                std::string name;
                std::weak_ptr<type> member_type = std::weak_ptr<type>();
            };

        private:
            std::optional<std::string> m_name;
            std::vector<member> m_members;

            friend class type_context;
        public:
            union_type(std::optional<std::string>&& name, std::vector<member>&& members)
                : m_name(std::move(name)), m_members(std::move(members)) {}

            const std::optional<std::string>& name() const noexcept { return m_name; }
            const std::vector<member>& members() const noexcept { return m_members; }

            bool is_assignable_from(const type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const union_type& other_union = static_cast<const union_type&>(other);
                if (m_name != other_union.name()) {
                    return false;
                }

                if (members().size() != other_union.members().size()) {
                    return false;
                }

                std::map<std::string, const type*> member_types;
                for (const auto& member : members()) {
                    member_types[member.name] = member.member_type.lock().get();
                }
                for (const auto& member : other_union.members()) {
                    auto it = member_types.find(member.name);
                    if (it == member_types.end()) {
                        return false;
                    }

                    if (!are_quivalent(*it->second, *member.member_type.lock())) {
                        return false;
                    }
                }

                return true;
            }

            bool implemented() const noexcept { 
                for (const auto& member : members()) {
                    if (member.member_type.lock()) {
                        return false;
                    }
                }
                return true;
            }

            bool implement_member_types(std::vector<std::shared_ptr<type>>&& member_types) {
                if (implemented() || member_types.size() != m_members.size()) {
                    return false;
                }

                for (size_t i = 0; i < m_members.size(); i++) {
                    m_members[i].member_type = std::weak_ptr<type>(member_types[i]);
                }
                return true;
            }

            std::string to_string() const override {
                if (m_name.has_value()) {
                    return "union " + m_name.value();
                }
                std::ostringstream ss;
                ss << "union { ";
                for (const auto& m : m_members) {
                    ss << m.member_type.lock()->to_string() << " " << m.name << "; ";
                }
                ss << "}";
                return ss.str();
            }
        };

        class enum_type final : public type {
        public:
            struct enumerator {
                std::string name;
                int64_t value;
            };

        private:

            std::optional<std::string> m_name;
            std::vector<enumerator> m_enumerators;

        public:
            enum_type(std::optional<std::string>&& name, std::vector<enumerator>&& enumerators)
                : m_name(std::move(name)), m_enumerators(std::move(enumerators)) {}

            const std::optional<std::string>& name() const noexcept { return m_name; }
            const std::vector<enumerator>& enumerators() const noexcept { return m_enumerators; }

            bool is_assignable_from(const type& other) const override {
                if (typeid(other) != typeid(*this)) {
                    return false;
                }

                const enum_type& other_enum = static_cast<const enum_type&>(other);
                if (m_name != other_enum.name()) {
                    return false;
                }

                if (enumerators().size() != other_enum.enumerators().size()) {
                    return false;
                }
                for (size_t i = 0; i < enumerators().size(); i++) {
                    if (enumerators()[i].value != other_enum.enumerators()[i].value) {
                        return false;
                    }
                }

                return true;
            }

            std::string to_string() const override {
                if (m_name.has_value()) {
                    return "enum " + m_name.value();
                }
                std::ostringstream ss;
                ss << "enum { ";
                for (size_t i = 0; i < m_enumerators.size(); i++) {
                    if (i > 0) ss << ", ";
                    ss << m_enumerators[i].name << " = " << m_enumerators[i].value;
                }
                ss << " }";
                return ss.str();
            }
        };
    }
}
#endif