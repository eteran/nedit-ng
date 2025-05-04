
#include "SmartIndent.h"
#include "DialogSmartIndent.h"
#include "DialogSmartIndentCommon.h"
#include "Preferences.h"
#include "Settings.h"
#include "Shift.h"
#include "SmartIndentEntry.h"
#include "Util/Input.h"
#include "Util/Raise.h"
#include "Util/Resource.h"
#include "Util/algorithm.h"
#include "Yaml.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QtDebug>

#include <climits>
#include <cstring>

#include <yaml-cpp/yaml.h>

namespace SmartIndent {

QPointer<DialogSmartIndent> SmartIndentDialog;
std::vector<SmartIndentEntry> SmartIndentSpecs;
QString CommonMacros;

namespace {

/**
 * @brief Read a macro (arbitrary text terminated by the macro end boundary string)
 * trim off added tabs and return the string.
 *
 * @param in Input object containing the macro text.
 * @return The macro text with leading tabs removed, or an empty string if the macro end boundary is not found.
 */
QString ReadSmartIndentMacro(Input &in) {

	static const auto MacroEndBoundary = QStringLiteral("--End-of-Macro--");

	// Strip leading newline
	if (*in == QLatin1Char('\n')) {
		++in;
	}

	// Find the end of the macro
	const int macroEnd = in.find(MacroEndBoundary);
	if (macroEnd == -1) {
		return QString();
	}

	// Copy the macro
	const QString macroStr = in.mid(macroEnd - in.index());

	// Remove leading tabs added by writer routine
	in += macroEnd - in.index();
	in += MacroEndBoundary.size();

	return ShiftText(macroStr, ShiftDirection::Left, /*tabsAllowed=*/true, /*tabDist=*/8, /*nChars=*/8);
}

/**
 * @brief Check if the given indent specification is the default one for its language.
 *
 * @param indentSpec The indent specification to check.
 * @return `true` if the indent specification is the default for its language, `false` otherwise.
 */
bool IsDefaultIndentSpec(const SmartIndentEntry *indentSpec) {

	if (const SmartIndentEntry *spec = FindDefaultIndentSpec(indentSpec->language)) {
		return (*spec == *indentSpec);
	}

	return false;
}

/**
 * @brief Load the default smart indent specifications from the resource file.
 *
 * @return A vector of `SmartIndentEntry` objects containing the default indent specifications.
 */
std::vector<SmartIndentEntry> LoadDefaultIndentSpecs() {
	static QByteArray defaultIndentRules = LoadResource(QLatin1String("DefaultSmartIndent.yaml"));
	static YAML::Node indentRules        = YAML::Load(defaultIndentRules.data());

	std::vector<SmartIndentEntry> specs;

	for (auto it = indentRules.begin(); it != indentRules.end(); ++it) {
		const std::string key  = it->first.as<std::string>();
		const YAML::Node value = it->second;

		if (key == "languages") {

			for (auto lang_it = value.begin(); lang_it != value.end(); ++lang_it) {

				SmartIndentEntry is;
				is.language = lang_it->first.as<QString>();

				YAML::Node entries = lang_it->second;

				if (entries.IsMap()) {
					for (auto set_it = entries.begin(); set_it != entries.end(); ++set_it) {
						const std::string key  = set_it->first.as<std::string>();
						const YAML::Node value = set_it->second;

						if (key == "on_init") {
							is.initMacro = value.as<QString>();
						} else if (key == "on_newline") {
							is.newlineMacro = value.as<QString>();
						} else if (key == "on_modification") {
							is.modMacro = value.as<QString>();
						}
					}
				}

				Upsert(specs, is, [&is](const SmartIndentEntry &entry) {
					return entry.language == is.language;
				});
			}
		}
	}

	return specs;
}

}

/**
 * @brief Load the default common macros from the resource file.
 *
 * @return The common macros, or an empty string if not found.
 */
QString LoadDefaultCommonMacros() {
	static QByteArray defaultIndentRules = LoadResource(QLatin1String("DefaultSmartIndent.yaml"));
	static YAML::Node indentRules        = YAML::Load(defaultIndentRules.data());

	for (auto it = indentRules.begin(); it != indentRules.end(); ++it) {
		const std::string key  = it->first.as<std::string>();
		const YAML::Node value = it->second;

		if (key == "common") {
			return value.as<QString>();
		}
	}

	return QString();
}

/**
 * @brief Find the default indent specification for a given language mode name.
 *
 * @param language The language mode name to search for.
 * @return The `SmartIndentEntry` for the default indent specification, or `nullptr` if not found.
 */
const SmartIndentEntry *FindDefaultIndentSpec(const QString &language) {

	if (language.isNull()) {
		return nullptr;
	}

	static const std::vector<SmartIndentEntry> defaultSpecs = LoadDefaultIndentSpecs();

	for (const SmartIndentEntry &spec : defaultSpecs) {
		if (spec.language == language) {
			return &spec;
		}
	}

	return nullptr;
}

/**
 * @brief Check if smart indent macros are available for a given language mode name.
 *
 * @param languageMode The name of the language mode to check for smart indent macros.
 * @return `true` if there are smart indent macros for a named language.
 */
bool SmartIndentMacrosAvailable(const QString &languageMode) {
	return FindIndentSpec(languageMode) != nullptr;
}

/**
 * @brief Load smart indent specifications from a YAML file.
 */
void LoadSmartIndentFromYaml() {

	struct ParseError {
		QString message;
	};

	try {
		YAML::Node indentRules;

		const QString filename = Settings::SmartIndentFile();
		if (QFileInfo::exists(filename)) {
			indentRules = YAML::LoadFile(filename.toUtf8().data());
		} else {
			static QByteArray defaultIndentRules = LoadResource(QLatin1String("DefaultSmartIndent.yaml"));
			indentRules                          = YAML::Load(defaultIndentRules.data());
		}

		for (auto it = indentRules.begin(); it != indentRules.end(); ++it) {
			const std::string key  = it->first.as<std::string>();
			const YAML::Node value = it->second;

			if (key == "common") {
				CommonMacros = value.as<QString>();
				if (CommonMacros == QStringLiteral("Default")) {
					CommonMacros = LoadDefaultCommonMacros();
				}
			} else if (key == "languages") {
				for (auto lang_it = value.begin(); lang_it != value.end(); ++lang_it) {

					SmartIndentEntry is;
					is.language        = lang_it->first.as<QString>();
					YAML::Node entries = lang_it->second;

					if (entries.IsMap()) {
						for (auto set_it = entries.begin(); set_it != entries.end(); ++set_it) {
							const std::string key  = set_it->first.as<std::string>();
							const YAML::Node value = set_it->second;

							if (key == "on_init") {
								is.initMacro = value.as<QString>();
							} else if (key == "on_newline") {
								is.newlineMacro = value.as<QString>();
							} else if (key == "on_modification") {
								is.modMacro = value.as<QString>();
							}
						}
					} else if (entries.as<std::string>() == "Default") {
						/* look for "Default" keyword, and if it's there, return the default
						   smart indent macros */
						const SmartIndentEntry *spec = FindDefaultIndentSpec(is.language);
						if (!spec) {
							Raise<ParseError>(tr("no default smart indent macro for language: %1").arg(is.language));
						}

						is = *spec;
					}

					Upsert(SmartIndentSpecs, is, [&is](const SmartIndentEntry &entry) {
						return entry.language == is.language;
					});
				}
			}
		}
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Parse error in smart indent spec: %s", ex.what());
	} catch (const ParseError &ex) {
		qWarning("NEdit: Parse error in smart indent spec: %s", qPrintable(ex.message));
	}
}

/**
 * @brief Load smart indent specifications from a string.
 *
 * @param string The string containing the smart indent specifications.
 */
void LoadSmartIndentFromString(const QString &string) {

	struct ParseError {
		QString message;
	};

	Input in(&string);
	QString errMsg;

	try {
		for (;;) {
			SmartIndentEntry is;

			// skip over blank space
			in.skipWhitespaceNL();

			// finished
			if (in.atEnd()) {
				return;
			}

			// read language mode name
			is.language = Preferences::ReadSymbolicField(in);
			if (is.language.isNull()) {
				Raise<ParseError>(tr("language mode name required"));
			}

			if (!Preferences::SkipDelimiter(in, &errMsg)) {
				Raise<ParseError>(errMsg);
			}

			/* look for "Default" keyword, and if it's there, return the default
			   smart indent macros */
			if (in.match(QStringLiteral("Default"))) {
				/* look for "Default" keyword, and if it's there, return the default
				   smart indent macros */
				const SmartIndentEntry *spec = FindDefaultIndentSpec(is.language);
				if (!spec) {
					Raise<ParseError>(tr("no default smart indent macro for language: %1").arg(is.language));
				}

				is = *spec;
			} else {

				/* read the initialization macro (arbitrary text terminated by the
				   macro end boundary string) */
				is.initMacro = ReadSmartIndentMacro(in);
				if (is.initMacro.isNull()) {
					Raise<ParseError>(tr("no end boundary to initialization macro"));
				}

				// read the newline macro
				is.newlineMacro = ReadSmartIndentMacro(in);
				if (is.newlineMacro.isNull()) {
					Raise<ParseError>(tr("no end boundary to newline macro"));
				}

				// read the modify macro
				is.modMacro = ReadSmartIndentMacro(in);
				if (is.modMacro.isNull()) {
					Raise<ParseError>(tr("no end boundary to modify macro"));
				}

				// if there's no mod macro, make it null so it won't be executed
				if (is.modMacro.isEmpty()) {
					is.modMacro = QString();
				}
			}

			Upsert(SmartIndentSpecs, is, [&is](const SmartIndentEntry &entry) {
				return entry.language == is.language;
			});
		}
	} catch (const ParseError &e) {
		Preferences::ReportError(
			nullptr,
			*in.string(),
			in.index(),
			tr("smart indent specification"),
			e.message);
	}
}

/**
 * @brief Load smart indent specifications from a string or a file.
 *
 * @param string The string containing the smart indent specifications, or "*" to load from the YAML file.
 */
void LoadSmartIndentString(const QString &string) {

	if (string == QLatin1String("*")) {
		LoadSmartIndentFromYaml();
	} else {
		LoadSmartIndentFromString(string);
	}
}

/**
 * @brief Load the common smart indent string from a given string.
 *
 * @param string The string containing the common smart indent macros.
 *               If the string is "Default", the default common macros are loaded.
 *               If the string is "*", it indicates that the common macros should be loaded
 *               from the YAML file, which already happens in `LoadSmartIndentString`.
 *
 */
void LoadSmartIndentCommonString(const QString &string) {

	if (string != QLatin1String("*")) {
		return;
	}

	Input in(&string);

	// skip over blank space
	in.skipWhitespaceNL();

	/* look for "Default" keyword, and if it's there, return the default
	   smart common macro */
	if (in.match(QStringLiteral("Default"))) {
		CommonMacros = LoadDefaultCommonMacros();
		return;
	}

	// Remove leading tabs added by writer routine
	CommonMacros = ShiftText(in.mid(), ShiftDirection::Left, /*tabsAllowed=*/true, /*tabDist=*/8, /*nChars*/ 8);
}

/**
 * @brief Writes the smart indent specifications to a YAML file.
 *
 * @return The string that should be written to the global preferences file.
 *         If the specifications are successfully written, it returns "*".
 *         If an error occurs during writing, it returns an empty string.
 */
QString WriteSmartIndentString() {
	const QString filename = Settings::SmartIndentFile();
	try {
		YAML::Emitter out;

		out << YAML::BeginMap;

		static const QString defaults = LoadDefaultCommonMacros();
		if (CommonMacros == defaults) {
			out << YAML::Key << "common" << YAML::Value << "Default";
		} else {
			out << YAML::Key << "common" << YAML::Value << YAML::Literal << CommonMacros.toUtf8().data();
		}

		out << YAML::Key << "languages" << YAML::Value;

		out << YAML::BeginMap;
		for (const SmartIndentEntry &sis : SmartIndentSpecs) {
			out << YAML::Key << sis.language.toUtf8().data() << YAML::Value;
			if (IsDefaultIndentSpec(&sis)) {
				out << "Default";
			} else {
				out << YAML::BeginMap;
				if (!sis.initMacro.isEmpty()) {
					out << YAML::Key << "on_init" << YAML::Value << YAML::Literal << sis.initMacro.toUtf8().data();
				}

				if (!sis.newlineMacro.isEmpty()) {
					out << YAML::Key << "on_newline" << YAML::Value << YAML::Literal << sis.newlineMacro.toUtf8().data();
				}

				if (!sis.modMacro.isEmpty()) {
					out << YAML::Key << "on_modification" << YAML::Value << YAML::Literal << sis.modMacro.toUtf8().data();
				}

				out << YAML::EndMap;
			}
		}
		out << YAML::EndMap;
		out << YAML::EndMap;

		QFile file(filename);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(out.c_str());
			file.write("\n");
		}

		return QLatin1String("*");
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Error writing %s in config directory:\n%s", qPrintable(filename), ex.what());
	}

	return QString();
}

/**
 * @brief Returns a common smart indent string.
 *
 * @return A string representing the common smart indent macros.
 *         If the common macros are not set, it returns a default string "*".
 */
QString WriteSmartIndentCommonString() {
	return QLatin1String("*");
}

/**
 * @brief Find the smart indent specification for a given language mode.
 *
 * @param language The language mode to search for in the smart indent specifications.
 * @return The `SmartIndentEntry` for the specified language mode,
 * or `nullptr` if no specification is found.
 */
const SmartIndentEntry *FindIndentSpec(const QString &language) {

	if (language.isNull()) {
		return nullptr;
	}

	for (const SmartIndentEntry &spec : SmartIndentSpecs) {
		if (spec.language == language) {
			return &spec;
		}
	}

	return nullptr;
}

/**
 * @brief Check if there are smart indent macros for a given language mode.
 *
 * @param languageMode The language mode to check for smart indent macros.
 * @return `true` if there are smart indent macros for the language mode,
 * or potential macros not yet committed in the smart indent dialog for a
 * language mode, `false` otherwise.
 */
bool LMHasSmartIndentMacros(const QString &languageMode) {
	if (FindIndentSpec(languageMode) != nullptr) {
		return true;
	}

	return SmartIndentDialog && SmartIndentDialog->languageMode() == languageMode;
}

/**
 * @brief Rename smart indent macros for a specific language mode.
 * This function updates the language name in the smart indent specifications
 * and the dialog if it is currently open.
 *
 * @param oldName The old language mode name to be replaced.
 * @param newName The new language mode name to set.
 */
void RenameSmartIndentMacros(const QString &oldName, const QString &newName) {

	auto it = std::find_if(SmartIndentSpecs.begin(), SmartIndentSpecs.end(), [&oldName](const SmartIndentEntry &entry) {
		return entry.language == oldName;
	});

	if (it != SmartIndentSpecs.end()) {
		it->language = newName;
	}

	if (SmartIndentDialog) {
		if (SmartIndentDialog->languageMode() == oldName) {
			SmartIndentDialog->setLanguageMode(newName);
		}
	}
}

/**
 * @brief Update the language mode menu in the smart indent dialog.
 * If the dialog is open, it updates the language modes available for smart indent.
 */
void UpdateLangModeMenuSmartIndent() {

	if (SmartIndentDialog) {
		SmartIndentDialog->updateLanguageModes();
	}
}

}
