
#ifndef DATA_VALUE_H_
#define DATA_VALUE_H_

#include <gsl/span>

#include <map>
#include <memory>
#include <string>
#include <system_error>

#include <boost/variant.hpp>
#include <boost/utility/string_view.hpp>

#include <QString>

class DocumentWidget;
struct DataValue;
struct Program;
union Inst;

using Arguments      = gsl::span<DataValue>;
using LibraryRoutine = std::error_code (*)(DocumentWidget *document, Arguments arguments, DataValue *result);
using Array          = std::map<std::string, DataValue>;
using ArrayPtr       = std::shared_ptr<Array>;

// we use a kind of "fat iterator", because the arrayIter function
// needs to know if the iterator is at the end of the map. This requirement
// means that we need a reference to the map to compare against
struct ArrayIterator {
	ArrayPtr m;
	Array::iterator it;
};

using Data = boost::variant<
	boost::blank,
	int32_t,
	std::string,
	ArrayPtr,
	ArrayIterator,
	LibraryRoutine,
	Program *,
	Inst *,
	DataValue *>;

struct DataValue {
	Data value;
};

// accessors
inline DataValue make_value(const ArrayPtr &map) {
	DataValue DV;
	DV.value = map;
	return DV;
}

inline DataValue make_value(const ArrayIterator &iter) {
	DataValue DV;
	DV.value = iter;
	return DV;
}

inline DataValue make_value() {
	DataValue DV;
	return DV;
}

inline DataValue make_value(int32_t n) {
	DataValue DV;
	DV.value = n;
	return DV;
}

inline DataValue make_value(int64_t n) {
	DataValue DV;
	if (n > std::numeric_limits<int32_t>::max() || n < std::numeric_limits<int32_t>::min()) {
		qWarning("A value being stored in a macro variable will be truncated");
	}
	DV.value = static_cast<int32_t>(n);
	return DV;
}

inline DataValue make_value(bool n) {
	DataValue DV;
	DV.value = n ? 1 : 0;
	return DV;
}

inline DataValue make_value(boost::string_view str) {
	DataValue DV;
	DV.value = str.to_string();
	return DV;
}

inline DataValue make_value(const QString &str) {
	DataValue DV;
	DV.value = str.toStdString();
	return DV;
}

inline DataValue make_value(Program *prog) {
	DataValue DV;
	DV.value = prog;
	return DV;
}

inline DataValue make_value(Inst *inst) {
	DataValue DV;
	DV.value = inst;
	return DV;
}

inline DataValue make_value(DataValue *v) {
	DataValue DV;
	DV.value = v;
	return DV;
}

inline DataValue make_value(LibraryRoutine routine) {
	DataValue DV;
	DV.value = routine;
	return DV;
}

inline bool is_unset(const DataValue &dv) {
	return dv.value.which() == 0;
}

inline bool is_integer(const DataValue &dv) {
	return dv.value.which() == 1;
}

inline bool is_string(const DataValue &dv) {
	return dv.value.which() == 2;
}

inline bool is_array(const DataValue &dv) {
	return dv.value.which() == 3;
}

inline std::string to_string(const DataValue &dv) {

	if (auto n = boost::get<int>(&dv.value)) {
		return std::to_string(*n);
	} else {
		return boost::get<std::string>(dv.value);
	}
}

inline int to_integer(const DataValue &dv) {
	return boost::get<int>(dv.value);
}

inline Program *to_program(const DataValue &dv) {
	return boost::get<Program *>(dv.value);
}

inline LibraryRoutine to_subroutine(const DataValue &dv) {
	return boost::get<LibraryRoutine>(dv.value);
}

inline DataValue *to_data_value(const DataValue &dv) {
	return boost::get<DataValue *>(dv.value);
}

inline Inst *to_instruction(const DataValue &dv) {
	return boost::get<Inst *>(dv.value);
}

inline ArrayPtr to_array(const DataValue &dv) {
	return boost::get<ArrayPtr>(dv.value);
}

inline ArrayIterator to_iterator(const DataValue &dv) {
	return boost::get<ArrayIterator>(dv.value);
}

#endif
