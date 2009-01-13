#include <matcher.h>
#include <logger.h>
#include <utils.h>
#include <exceptions.h>

#include <sys/time.h>
#include <regex.h>
#include <ctime>
#include <cassert>
#include <sstream>
#include <vector>

namespace newsbeuter {

matchable::matchable() { }
matchable::~matchable() { }

matcher::matcher() { }

matcher::matcher(const std::string& expr) {
	parse(expr);
}

bool matcher::parse(const std::string& expr) {
	struct timeval tv1, tv2;
	gettimeofday(&tv1, NULL);

	bool b = p.parse_string(expr);

	if (!b) {
		errmsg = utils::wstr2str(p.get_error());
	}

	gettimeofday(&tv2, NULL);
	unsigned long diff = (((tv2.tv_sec - tv1.tv_sec) * 1000000) + tv2.tv_usec) - tv1.tv_usec;
	GetLogger().log(LOG_DEBUG, "matcher::parse: parsing `%s' took %lu µs (success = %d)", expr.c_str(), diff, b ? 1 : 0);

	return b;
}

bool matcher::matches(matchable* item) {
	/*
	 * with this method, every class that is derived from matchable can be
	 * matched against a filter expression that was previously passed to the
	 * class with the parse() method.
	 *
	 * This makes it easy to use the matcher virtually everywhere, since C++
	 * allows multiple inheritance (i.e. deriving from matchable can even be
	 * used in class hierarchies), and deriving from matchable means that you
	 * only have to implement two methods has_attribute() and get_attribute().
	 *
	 * The whole matching code is speed-critical, as the matching happens on a
	 * lot of different occassions, and slow matching can be easily measured
	 * (and felt by the user) on slow computers with a lot of items to match.
	 */
	bool retval = false; 
	if (item) { 
		scope_measure m1("matcher::matches");
		retval = matches_r(p.get_root(), item);
	}
	return retval;
}

bool matcher::matchop_lt(expression * e, matchable * item) {
	if (!item->has_attribute(e->name))
		throw matcherexception(matcherexception::ATTRIB_UNAVAIL, e->name);
	std::istringstream islit(e->literal);
	std::istringstream isatt(item->get_attribute(e->name));
	int ilit, iatt;
	islit >> ilit;
	isatt >> iatt;
	return iatt < ilit;
}

bool matcher::matchop_gt(expression * e, matchable * item) {
	if (!item->has_attribute(e->name))
		throw matcherexception(matcherexception::ATTRIB_UNAVAIL, e->name);
	std::istringstream islit(e->literal);
	std::istringstream isatt(item->get_attribute(e->name));
	int ilit, iatt;
	islit >> ilit;
	isatt >> iatt;
	return iatt > ilit;
}

bool matcher::matchop_rxeq(expression * e, matchable * item) {
	if (!item->has_attribute(e->name))
		throw matcherexception(matcherexception::ATTRIB_UNAVAIL, e->name);
	if (!e->regex) {
		e->regex = new regex_t;
		regcomp(e->regex, e->literal.c_str(), REG_EXTENDED | REG_ICASE | REG_NOSUB); // TODO: see below
	}
	if (regexec(e->regex, item->get_attribute(e->name).c_str(), 0, NULL, 0)==0)
		return true;
	return false;
}

bool matcher::matchop_cont(expression * e, matchable * item) {
	if (!item->has_attribute(e->name))
		throw matcherexception(matcherexception::ATTRIB_UNAVAIL, e->name);
	std::vector<std::string> elements = utils::tokenize(item->get_attribute(e->name), " ");
	std::string literal = e->literal;
	for (std::vector<std::string>::iterator it=elements.begin();it!=elements.end();++it) {
		if (literal == *it) {
			return true;
		}
	}
	return false;
}

bool matcher::matchop_eq(expression * e, matchable * item) {
	if (!item->has_attribute(e->name)) {
		GetLogger().log(LOG_WARN, "matcher::matches_r: attribute %s not available", e->name.c_str());
		throw matcherexception(matcherexception::ATTRIB_UNAVAIL, e->name);
	}
	return (item->get_attribute(e->name)==e->literal);
}

bool matcher::matches_r(expression * e, matchable * item) {
	if (e) {
		switch (e->op) {
			/* the operator "and" and "or" simply connect two different subexpressions */
			case LOGOP_AND:
				return matches_r(e->l, item) && matches_r(e->r, item); // short-circuit evaulation in C -> short circuit evaluation in the filter language

			case LOGOP_OR:
				return matches_r(e->l, item) || matches_r(e->r, item); // same here

			/* while the other operator connect an attribute with a value */
			case MATCHOP_EQ:
				return matchop_eq(e, item);

			case MATCHOP_NE:
				return !matchop_eq(e, item);

			case MATCHOP_LT:
				return matchop_lt(e, item);

			case MATCHOP_GT:
				return matchop_gt(e, item);

			case MATCHOP_LE:
				return !matchop_gt(e, item);

			case MATCHOP_GE:
				return !matchop_lt(e, item);

			case MATCHOP_RXEQ:
				return matchop_rxeq(e, item);

			case MATCHOP_RXNE:
				return !matchop_rxeq(e, item);

			case MATCHOP_CONTAINS:
				return matchop_cont(e, item);

			case MATCHOP_CONTAINSNOT:
				return !matchop_cont(e, item);

			default:
				GetLogger().log(LOG_ERROR, "matcher::matches_r: invalid operator %d", e->op);
				assert(false); // that's an error condition
		}
		return false;
	} else {
		return true; // shouldn't happen
	}
}

const std::string& matcher::get_parse_error() {
	return errmsg;
}

}
