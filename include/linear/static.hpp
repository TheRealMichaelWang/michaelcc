#ifndef MICHAELCC_LINEAR_STATIC_HPP
#define MICHAELCC_LINEAR_STATIC_HPP

#include "logic/ir.hpp"

namespace michaelcc {
    namespace linear {
        class is_default_initialized : public logic::const_expression_dispatcher<bool> {
        protected:
            bool dispatch(const logic::integer_constant& node) override {
                return node.value() == 0;
            }
            bool dispatch(const logic::floating_constant& node) override {
                return node.value() == 0.0;
            }

            bool handle_default(const logic::expression&) override {
                return false;
            }
        };
    }
}

#endif