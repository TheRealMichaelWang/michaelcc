#include "errors.hpp"
#include <sstream>

namespace michaelcc {

std::string source_location::to_string() const {
	std::ostringstream ss;
	ss << m_file.string() << ":" << m_row << ":" << m_col;
	return ss.str();
}

compile_error::compile_error(std::string message, source_location location)
	: m_message(std::move(message)), m_location(std::move(location)) {
	m_formatted = m_location.to_string() + ": error: " + m_message;
}

}
