#include "semantic.hpp"
#include <sstream>

namespace michaelcc::semantic {

std::string int_type::str() const {
	std::string s;
	if (is_unsigned) s += "unsigned ";
	switch (k) {
	case kind::CHAR: s += "char"; break;
	case kind::SHORT: s += "short"; break;
	case kind::INT: s += "int"; break;
	case kind::LONG: s += "long"; break;
	case kind::LLONG: s += "long long"; break;
	}
	return s;
}

std::string float_type::str() const {
	switch (k) {
	case kind::FLOAT: return "float";
	case kind::DOUBLE: return "double";
	case kind::LDOUBLE: return "long double";
	}
	return "float";
}

std::string array_type::str() const {
	std::ostringstream ss;
	ss << elem->str() << "[" << count << "]";
	return ss.str();
}

std::string func_type::str() const {
	std::ostringstream ss;
	ss << ret->str() << "(";
	for (size_t i = 0; i < params.size(); ++i) {
		if (i) ss << ", ";
		ss << params[i]->str();
	}
	if (variadic) {
		if (!params.empty()) ss << ", ";
		ss << "...";
	}
	ss << ")";
	return ss.str();
}

bool func_type::eq(const type& o) const {
	auto* p = dynamic_cast<const func_type*>(&o);
	if (!p) return false;
	if (!ret->eq(*p->ret) || variadic != p->variadic) return false;
	if (params.size() != p->params.size()) return false;
	for (size_t i = 0; i < params.size(); ++i)
		if (!params[i]->eq(*p->params[i])) return false;
	return true;
}

}
