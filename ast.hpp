#pragma once

#include <map>
#include <memory>
#include <vector>

namespace michaelcc::ast {
	class element {
	public:
		//std::unique_ptr<element> macro_expand(std::map<)
	};

	class context_block : public element {
	private:
		std::vector<std::unique_ptr<element>> m_statements;
	};

	class for_loop : public element {
	private:
		std::unique_ptr<element> m_initial_statement;
		std::unique_ptr<element> m_condition;
		std::unique_ptr<element> m_increment_statement;
	};

	class do_block : public element {
	private:
		std::unique_ptr<element> m_condition;
	};

	class while_block : public element {

	};

	class if_else_block : public element {

	};

	class integer_literal : public element {

	};

	class character_literal : public element {

	};
}