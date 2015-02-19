/*-------------------------------------------------------------------------
 *
 * value.cc
 * file description
 *
 * Copyright(c) 2015, CMU
 *
 * /n-store/src/common/value.cc
 *
 *-------------------------------------------------------------------------
 */

#include "common/value.h"

#include <cstdio>
#include <cstring>
#include <sstream>

namespace nstore {

// For x<op>y where x is an integer,
// promote x and y to s_intPromotionTable[y]
ValueType Value::s_intPromotionTable[] = {
		VALUE_TYPE_INVALID,   // 0 invalid
		VALUE_TYPE_NULL,      // 1 null
		VALUE_TYPE_INVALID,   // 2 <unused>
		VALUE_TYPE_BIGINT,    // 3 tinyint
		VALUE_TYPE_BIGINT,    // 4 smallint
		VALUE_TYPE_BIGINT,    // 5 integer
		VALUE_TYPE_BIGINT,    // 6 bigint
		VALUE_TYPE_INVALID,   // 7 <unused>
		VALUE_TYPE_DOUBLE,    // 8 double
		VALUE_TYPE_INVALID,   // 9 varchar
		VALUE_TYPE_INVALID,   // 10 <unused>
		VALUE_TYPE_BIGINT,    // 11 timestamp

		// 12 - 21 unused
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,

		VALUE_TYPE_DECIMAL,   // 22 decimal
		VALUE_TYPE_INVALID,   // 23 boolean
		VALUE_TYPE_INVALID,   // 24 address
};

// for x<op>y where x is a double
// promote x and y to s_doublePromotionTable[y]
ValueType Value::s_doublePromotionTable[] = {
		VALUE_TYPE_INVALID,   // 0 invalid
		VALUE_TYPE_NULL,      // 1 null
		VALUE_TYPE_INVALID,   // 2 <unused>
		VALUE_TYPE_DOUBLE,    // 3 tinyint
		VALUE_TYPE_DOUBLE,    // 4 smallint
		VALUE_TYPE_DOUBLE,    // 5 integer
		VALUE_TYPE_DOUBLE,    // 6 bigint
		VALUE_TYPE_INVALID,   // 7 <unused>
		VALUE_TYPE_DOUBLE,    // 8 double
		VALUE_TYPE_INVALID,   // 9 varchar
		VALUE_TYPE_INVALID,   // 10 <unused>
		VALUE_TYPE_DOUBLE,    // 11 timestamp

		// 12 - 21 unused.
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,

		VALUE_TYPE_INVALID,   // 22 decimal  (todo)
		VALUE_TYPE_INVALID,   // 23 boolean
		VALUE_TYPE_INVALID,   // 24 address
};

// for x<op>y where x is a decimal
// promote x and y to s_decimalPromotionTable[y]
ValueType Value::s_decimalPromotionTable[] = {
		VALUE_TYPE_INVALID,   // 0 invalid
		VALUE_TYPE_NULL,      // 1 null
		VALUE_TYPE_INVALID,   // 2 <unused>
		VALUE_TYPE_DECIMAL,   // 3 tinyint
		VALUE_TYPE_DECIMAL,   // 4 smallint
		VALUE_TYPE_DECIMAL,   // 5 integer
		VALUE_TYPE_DECIMAL,   // 6 bigint
		VALUE_TYPE_INVALID,   // 7 <unused>
		VALUE_TYPE_INVALID,   // 8 double (todo)
		VALUE_TYPE_INVALID,   // 9 varchar
		VALUE_TYPE_INVALID,   // 10 <unused>
		VALUE_TYPE_DECIMAL,   // 11 timestamp
		// 12 - 21 unused. ick.
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,
		VALUE_TYPE_INVALID, VALUE_TYPE_INVALID,
		VALUE_TYPE_DECIMAL,   // 22 decimal
		VALUE_TYPE_INVALID,   // 23 boolean
		VALUE_TYPE_INVALID,   // 24 address
};

TTInt Value::s_maxDecimal("9999999999"   //10 digits
		"9999999999"   //20 digits
		"9999999999"   //30 digits
		"99999999");    //38 digits

TTInt Value::s_minDecimal("-9999999999"   //10 digits
		"9999999999"   //20 digits
		"9999999999"   //30 digits
		"99999999");    //38 digits

/*
 * Produce a debugging string describing an Value.
 */
std::string Value::debug() const {
	const ValueType type = getValueType();
	if (isNull()) {
		return "<NULL>";
	}
	std::ostringstream buffer;
	std::string out_val;
	const char* ptr;
	int64_t addr;
	buffer << getTypeName(type) << "::";
	switch (type) {
	case VALUE_TYPE_TINYINT:
		buffer << static_cast<int32_t>(getTinyInt()); break;
	case VALUE_TYPE_SMALLINT:
		buffer << getSmallInt(); break;
	case VALUE_TYPE_INTEGER:
		buffer << getInteger(); break;
	case VALUE_TYPE_BIGINT:
	case VALUE_TYPE_TIMESTAMP:
		buffer << getBigInt();
		break;
	case VALUE_TYPE_DOUBLE:
		buffer << getDouble();
		break;
	case VALUE_TYPE_VARCHAR:
		ptr = reinterpret_cast<const char*>(getObjectValue());
		addr = reinterpret_cast<int64_t>(ptr);
		out_val = std::string(ptr, getObjectLength());
		buffer << "[" << getObjectLength() << "]";
		buffer << "\"" << out_val << "\"[@" << addr << "]";
		break;
	case VALUE_TYPE_VARBINARY:
		ptr = reinterpret_cast<const char*>(getObjectValue());
		addr = reinterpret_cast<int64_t>(ptr);
		out_val = std::string(ptr, getObjectLength());
		buffer << "[" << getObjectLength() << "]";
		buffer << "-bin[@" << addr << "]";
		break;
	case VALUE_TYPE_DECIMAL:
		buffer << createStringFromDecimal();
		break;
	default:
		throw UnknownTypeException((int)type, "unknown type");
	}
	std::string ret(buffer.str());
	return (ret);
}


/*
 * Convert ValueType to a string. One might say that,
 * strictly speaking, this has no business here.
 */
std::string Value::getTypeName(ValueType type) {
	std::string ret;
	switch (type) {
	case (VALUE_TYPE_TINYINT):
        				ret = "tinyint";
	break;
	case (VALUE_TYPE_SMALLINT):
        				ret = "smallint";
	break;
	case (VALUE_TYPE_INTEGER):
        				ret = "integer";
	break;
	case (VALUE_TYPE_BIGINT):
        				ret = "bigint";
	break;
	case (VALUE_TYPE_DOUBLE):
        				ret = "double";
	break;
	case (VALUE_TYPE_VARCHAR):
        				ret = "varchar";
	break;
	case (VALUE_TYPE_VARBINARY):
        				ret = "varbinary";
	break;
	case (VALUE_TYPE_TIMESTAMP):
        				ret = "timestamp";
	break;
	case (VALUE_TYPE_DECIMAL):
        				ret = "decimal";
	break;
	case (VALUE_TYPE_INVALID):
        				ret = "INVALID";
	break;
	case (VALUE_TYPE_NULL):
        				ret = "NULL";
	break;
	case (VALUE_TYPE_BOOLEAN):
        				ret = "boolean";
	break;
	case (VALUE_TYPE_ADDRESS):
        				ret = "address";
	break;
	default: {
		char buffer[32];
		snprintf(buffer, 32, "UNKNOWN[%d]", type);
		ret = buffer;
	}
	}
	return (ret);
}

/**
 * Serialize sign and value using radix point (no exponent).
 */
std::string Value::createStringFromDecimal() const {
	assert(!isNull());
	std::ostringstream buffer;
	TTInt scaledValue = getDecimal();
	if (scaledValue.IsSign()) {
		buffer << '-';
	}
	TTInt whole(scaledValue);
	TTInt fractional(scaledValue);
	whole /= Value::kMaxScaleFactor;
	fractional %= Value::kMaxScaleFactor;
	if (whole.IsSign()) {
		whole.ChangeSign();
	}
	buffer << whole.ToString(10);
	buffer << '.';
	if (fractional.IsSign()) {
		fractional.ChangeSign();
	}
	std::string fractionalString = fractional.ToString(10);
	for (int ii = static_cast<int>(fractionalString.size()); ii < Value::kMaxDecScale; ii++) {
		buffer << '0';
	}
	buffer << fractionalString;
	return buffer.str();
}

/**
 *   set a decimal value from a serialized representation
 */
void Value::createDecimalFromString(const std::string &txt) {
	if (txt.length() == 0) {
		throw DecimalException("Empty string provided");
	}
	bool setSign = false;
	if (txt[0] == '-') {
		setSign = true;
	}

	/**
	 * Check for invalid characters
	 */
	for (int ii = (setSign ? 1 : 0); ii < static_cast<int>(txt.size()); ii++) {
		if ((txt[ii] < '0' || txt[ii] > '9') && txt[ii] != '.') {
			char message[4096];
			snprintf(message, 4096, "Invalid characters in decimal string: %s",
					txt.c_str());
			throw DecimalException(message);
		}
	}

	std::size_t separatorPos = txt.find( '.', 0);
	if (separatorPos == std::string::npos) {
		const std::string wholeString = txt.substr( setSign ? 1 : 0, txt.size());
		const std::size_t wholeStringSize = wholeString.size();
		if (wholeStringSize > 26) {
			throw DecimalException("Maximum precision exceeded. "
					"Maximum of 26 digits to the left of the decimal point");
		}
		TTInt whole(wholeString);
		if (setSign) {
			whole.SetSign();
		}
		whole *= kMaxScaleFactor;
		getDecimal() = whole;
		return;
	}

	if (txt.find( '.', separatorPos + 1) != std::string::npos) {
		throw DecimalException("Too many decimal points");
	}

	const std::string wholeString = txt.substr( setSign ? 1 : 0, separatorPos - (setSign ? 1 : 0));
	const std::size_t wholeStringSize = wholeString.size();
	if (wholeStringSize > 26) {
		throw DecimalException("Maximum precision exceeded. "
				"Maximum of 26 digits to the left of the decimal point");
	}
	TTInt whole(wholeString);
	std::string fractionalString = txt.substr( separatorPos + 1, txt.size() - (separatorPos + 1));
	if (fractionalString.size() > 12) {
		throw DecimalException("Maximum scale exceeded. "
				"Maximum of 12 digits to the right of the decimal point");
	}
	while(fractionalString.size() < Value::kMaxDecScale) {
		fractionalString.push_back('0');
	}
	TTInt fractional(fractionalString);

	whole *= kMaxScaleFactor;
	whole += fractional;

	if (setSign) {
		whole.SetSign();
	}

	getDecimal() = whole;
}


/*
 * Avoid scaling both sides if possible. E.g, don't turn dec * 2 into
 * (dec * 2*kMaxScale*E-12). Then the result of simple multiplication
 * is a*b*E-24 and have to further multiply to get back to the assumed
 * E-12, which can overflow unnecessarily at the middle step.
 */
Value Value::opMultiplyDecimals(const Value &lhs, const Value &rhs) const {
	if ((lhs.getValueType() != VALUE_TYPE_DECIMAL) &&
			(rhs.getValueType() != VALUE_TYPE_DECIMAL))
	{
		throw DecimalException("No decimal Value in decimal multiply.");
	}

	if (lhs.isNull() || rhs.isNull()) {
		TTInt retval;
		retval.SetMin();
		return getDecimalValue( retval );
	}

	if ((lhs.getValueType() == VALUE_TYPE_DECIMAL) &&
			(rhs.getValueType() == VALUE_TYPE_DECIMAL))
	{
		TTLInt calc;
		calc.FromInt(lhs.getDecimal());
		calc *= rhs.getDecimal();
		calc /= Value::kMaxScaleFactor;
		TTInt retval;
		if (retval.FromInt(calc)  || retval > Value::s_maxDecimal || retval < s_minDecimal) {
			char message[4096];
			snprintf(message, 4096, "Attempted to multiply %s by %s causing overflow/underflow. Unscaled result was %s",
					lhs.createStringFromDecimal().c_str(), rhs.createStringFromDecimal().c_str(),
					calc.ToString(10).c_str());
		}
		return getDecimalValue(retval);
	} else if  (lhs.getValueType() != VALUE_TYPE_DECIMAL)
	{
		TTLInt calc;
		calc.FromInt(rhs.getDecimal());
		calc *= lhs.castAsDecimalAndGetValue();
		calc /= Value::kMaxScaleFactor;
		TTInt retval;
		retval.FromInt(calc);
		if (retval.FromInt(calc)  || retval > Value::s_maxDecimal || retval < s_minDecimal) {
			char message[4096];
			snprintf(message, 4096, "Attempted to multiply %s by %s causing overflow/underflow. Unscaled result was %s",
					lhs.createStringFromDecimal().c_str(), rhs.createStringFromDecimal().c_str(),
					calc.ToString(10).c_str());
			throw DecimalException(message);
		}
		return getDecimalValue(retval);
	}
	else
	{
		TTLInt calc;
		calc.FromInt(lhs.getDecimal());
		calc *= rhs.castAsDecimalAndGetValue();
		calc /= Value::kMaxScaleFactor;
		TTInt retval;
		retval.FromInt(calc);
		if (retval.FromInt(calc)  || retval > Value::s_maxDecimal || retval < s_minDecimal) {
			char message[4096];
			snprintf(message, 4096, "Attempted to multiply %s by %s causing overflow/underflow. Unscaled result was %s",
					lhs.createStringFromDecimal().c_str(), rhs.createStringFromDecimal().c_str(),
					calc.ToString(10).c_str());
			throw DecimalException(message);
		}
		return getDecimalValue(retval);
	}
}


/*
 * Divide two decimals and return a correctly scaled decimal.
 * A little cumbersome. Better algorithms welcome.
 *   (1) calculate the quotient and the remainder.
 *   (2) temporarily scale the remainder to 19 digits
 *   (3) divide out remainder to calculate digits after the radix point.
 *   (4) scale remainder to 12 digits (that's the default scale)
 *   (5) scale the quotient back to 19,12.
 *   (6) sum the scaled quotient and remainder.
 *   (7) construct the final decimal.
 */

Value Value::opDivideDecimals(const Value lhs, const Value rhs) const {
	if ((lhs.getValueType() != VALUE_TYPE_DECIMAL) ||
			(rhs.getValueType() != VALUE_TYPE_DECIMAL))
	{
		throw DecimalException("Non-decimal Value in decimal subtract.");
	}

	if (lhs.isNull() || rhs.isNull()) {
		TTInt retval;
		retval.SetMin();
		return getDecimalValue( retval );
	}

	TTLInt calc;
	calc.FromInt(lhs.getDecimal());
	calc *= Value::kMaxScaleFactor;
	if (calc.Div(rhs.getDecimal())) {
		char message[4096];
		snprintf( message, 4096, "Attempted to divide %s by %s causing overflow/underflow (or divide by zero)",
				lhs.createStringFromDecimal().c_str(), rhs.createStringFromDecimal().c_str());
		throw DecimalException(message);
	}
	TTInt retval;
	if (retval.FromInt(calc)  || retval > Value::s_maxDecimal || retval < s_minDecimal) {
		char message[4096];
		snprintf( message, 4096, "Attempted to divide %s by %s causing overflow. Unscaled result was %s",
				lhs.createStringFromDecimal().c_str(), rhs.createStringFromDecimal().c_str(),
				calc.ToString(10).c_str());
		throw DecimalException(message);
	}
	return getDecimalValue(retval);
}


} // End nstore namespace


