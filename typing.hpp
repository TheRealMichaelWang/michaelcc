#ifndef MICHAELCC_TYPING_HPP
#define MICHAELCC_TYPING_HPP

#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <map>

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

        class void_type;
        class int_type;
        class float_type;
        class pointer_type;
        class array_type;
        class function_pointer_type;

        class type {
        public:
            virtual ~type() = default;
            virtual std::unique_ptr<type> clone() const = 0;

            virtual bool is_assignable_from(const type* other) const = 0;
        };

        class void_type : public type {
        public:
            std::unique_ptr<type> clone() const override {
                return std::make_unique<void_type>();
            }

            bool is_assignable_from(const type* other) const override {
                return dynamic_cast<const void_type*>(other) != nullptr;
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

            std::unique_ptr<type> clone() const override {
                return std::make_unique<int_type>(m_qualifiers, m_class);
            }

            bool is_assignable_from(const type* other) const override {
                const int_type* other_int = dynamic_cast<const int_type*>(other);
                if (other_int == nullptr) {
                    return false;
                }
                return m_qualifiers == other_int->qualifiers() && m_class == other_int->type_class();
            }
        };

        class float_type : public type {
        private:
            float_class m_class;

        public:
            explicit float_type(float_class m_class)
                : m_class(m_class) { }

            float_class type_class() const noexcept { return m_class; }

            std::unique_ptr<type> clone() const override {
                return std::make_unique<float_type>(m_class);
            }

            bool is_assignable_from(const type* other) const override {
                const float_type* other_float = dynamic_cast<const float_type*>(other);
                if (other_float == nullptr) {
                    return false;
                }
                return m_class == other_float->type_class();
            }
        };

        class array_type : public type {
        private:
            std::unique_ptr<type> m_element_type;
            std::optional<std::unique_ptr<type>> m_length;

        public:
            array_type(std::unique_ptr<type>&& element_type, std::optional<std::unique_ptr<type>>&& length)
                : m_element_type(std::move(element_type)), m_length(std::move(length)) {}

            const type* element_type() const noexcept { return m_element_type.get(); }
            const std::optional<std::unique_ptr<type>>& length() const noexcept { return m_length; }

            std::unique_ptr<type> clone() const override {
                return std::make_unique<array_type>(m_element_type->clone(), m_length.has_value() ? std::make_optional(m_length.value()->clone()) : std::nullopt);
            }

            bool is_assignable_from(const type* other) const override {
                const array_type* other_arr = dynamic_cast<const array_type*>(other);
                if (other_arr == nullptr) {
                    return false;
                }

                if (dynamic_cast<const void_type*>(other_arr->element_type()) != nullptr) {
                    return true;
                }

                return m_element_type->is_assignable_from(other_arr->element_type());
            }
        };

        class function_pointer_type : public type {
        private:
            std::unique_ptr<type> m_return_type;
            std::vector<std::unique_ptr<type>> m_parameter_types;
        public:
            function_pointer_type(std::unique_ptr<type>&& return_type, std::vector<std::unique_ptr<type>>&& parameter_types)
                : m_return_type(std::move(return_type)), m_parameter_types(std::move(parameter_types)) {}
            const type* return_type() const noexcept { return m_return_type.get(); }
            const std::vector<std::unique_ptr<type>>& parameter_types() const noexcept { return m_parameter_types; }

            std::unique_ptr<type> clone() const override {
                std::vector<std::unique_ptr<type>> parameter_types;
                parameter_types.reserve(m_parameter_types.size());
                for (const auto& parameter : m_parameter_types) {
                    parameter_types.push_back(parameter->clone());
                }
                return std::make_unique<function_pointer_type>(m_return_type->clone(), std::move(parameter_types));
            }

            bool is_assignable_from(const type* other) const override {
                const function_pointer_type* other_fptr = dynamic_cast<const function_pointer_type*>(other);
                if (other_fptr == nullptr) {
                    return false;
                }
                
                for (size_t i = 0; i < m_parameter_types.size(); i++) {
                    if (!other_fptr->parameter_types()[i]->is_assignable_from(m_parameter_types[i].get())) {
                        return false;
                    }
                }

                return return_type()->is_assignable_from(other_fptr->return_type());
            }
        };

        class pointer_type : public type {
        private:
            std::unique_ptr<type> m_pointee_type;

        public:
            pointer_type(std::unique_ptr<type>&& pointee_type)
                : m_pointee_type(std::move(pointee_type)) { }

            const type* pointee_type() const noexcept { return m_pointee_type.get(); }

            std::unique_ptr<type> clone() const override {
                return std::make_unique<pointer_type>(m_pointee_type->clone());
            }

            bool is_assignable_from(const type* other) const override {
                const pointer_type* other_ptr = dynamic_cast<const pointer_type*>(other);
                if (other_ptr == nullptr) {
                    const array_type* other_arr = dynamic_cast<const array_type*>(other);
                    if (other_arr == nullptr) {
                        if (dynamic_cast<const void_type*>(pointee_type()) != nullptr
                            && dynamic_cast<const function_pointer_type*>(other_arr->element_type()) != nullptr) {
                            return true;
                        }

                        return false;
                    }

                    if (dynamic_cast<const void_type*>(other_arr->element_type()) != nullptr) {
                        return true;
                    }

                    return m_pointee_type->is_assignable_from(other_arr->element_type());
                }

                if (dynamic_cast<const void_type*>(other_ptr->pointee_type()) != nullptr) {
                    return true;
                }

                return m_pointee_type->is_assignable_from(other_ptr->pointee_type());
            }
        };

        class struct_type final : public type {
        public:
            struct field {
                std::string name;
                std::unique_ptr<type> field_type;
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

            std::unique_ptr<type> clone() const override {
                std::vector<field> fields;
                fields.reserve(m_fields.size());
                for (const auto& field : m_fields) {
                    fields.emplace_back(field.name, field.field_type->clone());
                }
                return std::make_unique<struct_type>(
                    m_name.has_value() ? std::make_optional(std::string(m_name.value())) : std::nullopt, 
                    std::move(fields)
                );
            }

            bool is_assignable_from(const type* other) const override {
                const struct_type* other_struct = dynamic_cast<const struct_type*>(other);
                if (other_struct == nullptr) {
                    return false;
                }

                if (m_name != other_struct->name()) {
                    return false;
                }

                if (fields().size() != other_struct->fields().size()) {
                    return false;
                }

                for (size_t i = 0; i < fields().size(); i++) {
                    if (!fields()[i].field_type->is_assignable_from(other_struct->fields()[i].field_type.get())) {
                        return false;
                    }
                }

                return true;
            }
        };

        class union_type final : public type {
        public:
            struct member {
                std::string name;
                std::unique_ptr<type> member_type;
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

            std::unique_ptr<type> clone() const override {
                std::vector<member> members;
                members.reserve(m_members.size());
                for (const auto& member : m_members) {
                    members.emplace_back(member.name, member.member_type->clone());
                }
                return std::make_unique<union_type>(
                    m_name.has_value() ? std::make_optional(std::string(m_name.value())) : std::nullopt,
                    std::move(members)
                );
            }

            bool is_assignable_from(const type* other) const override {
                const union_type* other_union = dynamic_cast<const union_type*>(other);
                if (other_union == nullptr) {
                    return false;
                }
                if (m_name != other_union->name()) {
                    return false;
                }

                if (members().size() != other_union->members().size()) {
                    return false;
                }

                std::map<std::string, const type*> member_types;
                for (const auto& member : members()) {
                    member_types[member.name] = member.member_type.get();
                }
                for (const auto& member : other_union->members()) {
                    auto it = member_types.find(member.name);
                    if (it == member_types.end()) {
                        return false;
                    }

                    if (!it->second->is_assignable_from(member.member_type.get())
                        && !member.member_type->is_assignable_from(it->second)) {
                        return false;
                    }
                }

                return true;
            }
        };
    }
}
#endif