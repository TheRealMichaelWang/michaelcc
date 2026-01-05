#include <sstream>
#include "errors.hpp"

using namespace michaelcc;

compilation_error::compilation_error(const std::string msg, const source_location location) : m_location(location), 
    m_msg(msg)
{
	std::stringstream ss;
	ss << msg << " @ " << m_location.to_string();
	m_display_msg = ss.str();
}

const std::string michaelcc::source_location::to_string() const
{
	std::stringstream ss;
	ss << "row " << m_row << ", col " << m_col << " in " << m_file_name << ".";
	return ss.str();
}
