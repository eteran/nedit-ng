
#include "SmartIndent.h"
#include "DialogSmartIndent.h"
#include "DialogSmartIndentCommon.h"
#include "Preferences.h"
#include "Settings.h"
#include "SmartIndentEntry.h"
#include "Util/Input.h"
#include "Util/Raise.h"
#include "Util/Resource.h"
#include "Util/algorithm.h"
#include "shift.h"

#include <yaml-cpp/yaml.h>

#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QtDebug>
#include <climits>
#include <cstring>

namespace SmartIndent {

QPointer<DialogSmartIndent> SmartIndentDialog;
std::vector<SmartIndentEntry> SmartIndentSpecs;
QString CommonMacros;

namespace {

/**
 * Read a macro (arbitrary text terminated by the macro end boundary string)
 * trim off added tabs and return the string. Returns QString() if the macro
 * end boundary string is not found.
 *
 * @brief readSmartIndentMacro
 * @param in
 * @return
 */
QString readSmartIndentMacro(Input &in) {

	static const auto MacroEndBoundary = QLatin1String("--End-of-Macro--");

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

	return shiftText(macroStr, ShiftDirection::Left, /*tabsAllowed=*/true, /*tabDist=*/8, /*nChars=*/8);
}

/**
 * @brief isDefaultIndentSpec
 * @param indentSpec
 * @return
 */
bool isDefaultIndentSpec(const SmartIndentEntry *indentSpec) {

	if (const SmartIndentEntry *spec = findDefaultIndentSpec(indentSpec->language)) {
		return (*spec == *indentSpec);
	}

	return false;
}

/**
 * @brief loadDefaultIndentSpecs
 * @return
 */
std::vector<SmartIndentEntry> loadDefaultIndentSpecs() {
	static QByteArray defaultIndentRules = loadResource(QLatin1String("DefaultSmartIndent.yaml"));
	static YAML::Node indentRules        = YAML::Load(defaultIndentRules.data());

	std::vector<SmartIndentEntry> specs;

	for (auto it = indentRules.begin(); it != indentRules.end(); ++it) {
		const std::string key  = it->first.as<std::string>();
		const YAML::Node value = it->second;

		if (key == "languages") {

			for (auto lang_it = value.begin(); lang_it != value.end(); ++lang_it) {

				SmartIndentEntry is;
				is.language = QString::fromUtf8(lang_it->first.as<std::string>().c_str());

				YAML::Node entries = lang_it->second;

				if (entries.IsMap()) {
					for (auto set_it = entries.begin(); set_it != entries.end(); ++set_it) {
						const std::string key  = set_it->first.as<std::string>();
						const YAML::Node value = set_it->second;

						if (key == "on_init") {
							is.initMacro = QString::fromUtf8(value.as<std::string>().c_str());
						} else if (key == "on_newline") {
							is.newlineMacro = QString::fromUtf8(value.as<std::string>().c_str());
						} else if (key == "on_modification") {
							is.modMacro = QString::fromUtf8(value.as<std::string>().c_str());
						}
					}
				}

				insert_or_replace(specs, is, [&is](const SmartIndentEntry &entry) {
					return entry.language == is.language;
				});
			}
		}
	}

	return specs;
}

}

/**
 * @brief loadDefaultCommonMacros
 * @return
 */
QString loadDefaultCommonMacros() {
	static QByteArray defaultIndentRules = loadResource(QLatin1String("DefaultSmartIndent.yaml"));
	static YAML::Node indentRules        = YAML::Load(defaultIndentRules.data());

	for (auto it = indentRules.begin(); it != indentRules.end(); ++it) {
		const std::string key  = it->first.as<std::string>();
		const YAML::Node value = it->second;

		if (key == "common") {
			return QString::fromUtf8(value.as<std::string>().c_str());
		}
	}

	return QString();
}

/**
 * @brief findDefaultIndentSpec
 * @param language
 * @return
 */
const SmartIndentEntry *findDefaultIndentSpec(const QString &language) {

	if (language.isNull()) {
		return nullptr;
	}

	static const std::vector<SmartIndentEntry> defaultSpecs = loadDefaultIndentSpecs();

	auto it = std::find_if(defaultSpecs.begin(), defaultSpecs.end(), [&language](const SmartIndentEntry &spec) {
		return language == spec.language;
	});

	if (it != defaultSpecs.end()) {
		return &*it;
	}

	return nullptr;
}

/*
** Returns true if there are smart indent macros for a named language
*/
bool smartIndentMacrosAvailable(const QString &languageModeName) {
	return findIndentSpec(languageModeName) != nullptr;
}

/**
 * @brief loadSmartIndentString
 * @param string
 * @return
 */
void loadSmartIndentString(const QString &string) {

	struct ParseError {
		QString message;
	};

	if (string == QLatin1String("*")) {
		try {
			YAML::Node indentRules;

			const QString filename = Settings::smartIndentFile();
			if (QFileInfo::exists(filename)) {
				indentRules = YAML::LoadFile(filename.toUtf8().data());
			} else {
				static QByteArray defaultIndentRules = loadResource(QLatin1String("DefaultSmartIndent.yaml"));
				indentRules                          = YAML::Load(defaultIndentRules.data());
			}

			for (auto it = indentRules.begin(); it != indentRules.end(); ++it) {
				const std::string key  = it->first.as<std::string>();
				const YAML::Node value = it->second;

				if (key == "common") {
					CommonMacros = QString::fromUtf8(value.as<std::string>().c_str());
					if (CommonMacros == QLatin1String("Default")) {
						CommonMacros = loadDefaultCommonMacros();
					}
				} else if (key == "languages") {
					for (auto lang_it = value.begin(); lang_it != value.end(); ++lang_it) {

						SmartIndentEntry is;
						is.language        = QString::fromUtf8(lang_it->first.as<std::string>().c_str());
						YAML::Node entries = lang_it->second;

						if (entries.IsMap()) {
							for (auto set_it = entries.begin(); set_it != entries.end(); ++set_it) {
								const std::string key  = set_it->first.as<std::string>();
								const YAML::Node value = set_it->second;

								if (key == "on_init") {
									is.initMacro = QString::fromUtf8(value.as<std::string>().c_str());
								} else if (key == "on_newline") {
									is.newlineMacro = QString::fromUtf8(value.as<std::string>().c_str());
								} else if (key == "on_modification") {
									is.modMacro = QString::fromUtf8(value.as<std::string>().c_str());
								}
							}
						} else if (entries.as<std::string>() == "Default") {
							/* look for "Default" keyword, and if it's there, return the default
							   smart indent macros */
							const SmartIndentEntry *spec = findDefaultIndentSpec(is.language);
							if (!spec) {
								Raise<ParseError>(tr("no default smart indent macro for language: %1").arg(is.language));
							}

							is = *spec;
						}

						insert_or_replace(SmartIndentSpecs, is, [&is](const SmartIndentEntry &entry) {
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
	} else {

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
				if (in.match(QLatin1String("Default"))) {
					/* look for "Default" keyword, and if it's there, return the default
					   smart indent macros */
					const SmartIndentEntry *spec = findDefaultIndentSpec(is.language);
					if (!spec) {
						Raise<ParseError>(tr("no default smart indent macro for language: %1").arg(is.language));
					}

					is = *spec;
				} else {

					/* read the initialization macro (arbitrary text terminated by the
					   macro end boundary string) */
					is.initMacro = readSmartIndentMacro(in);
					if (is.initMacro.isNull()) {
						Raise<ParseError>(tr("no end boundary to initialization macro"));
					}

					// read the newline macro
					is.newlineMacro = readSmartIndentMacro(in);
					if (is.newlineMacro.isNull()) {
						Raise<ParseError>(tr("no end boundary to newline macro"));
					}

					// read the modify macro
					is.modMacro = readSmartIndentMacro(in);
					if (is.modMacro.isNull()) {
						Raise<ParseError>(tr("no end boundary to modify macro"));
					}

					// if there's no mod macro, make it null so it won't be executed
					if (is.modMacro.isEmpty()) {
						is.modMacro = QString();
					}
				}

				insert_or_replace(SmartIndentSpecs, is, [&is](const SmartIndentEntry &entry) {
					return entry.language == is.language;
				});
			}
		} catch (const ParseError &e) {
			Preferences::reportError(
				nullptr,
				*in.string(),
				in.index(),
				tr("smart indent specification"),
				e.message);
		}
	}
}

/**
 * @brief loadSmartIndentCommonString
 * @param string
 */
void loadSmartIndentCommonString(const QString &string) {

	if (string != QLatin1String("*")) {
		Input in(&string);

		// skip over blank space
		in.skipWhitespaceNL();

		/* look for "Default" keyword, and if it's there, return the default
		   smart common macro */
		if (in.match(QLatin1String("Default"))) {
			CommonMacros = loadDefaultCommonMacros();
			return;
		}

		// Remove leading tabs added by writer routine
		CommonMacros = shiftText(in.mid(), ShiftDirection::Left, /*tabsAllowed=*/true, /*tabDist=*/8, /*nChars*/ 8);
	}
}

/**
 * @brief writeSmartIndentString
 * @return
 */
QString writeSmartIndentString() {
	const QString filename = Settings::smartIndentFile();
	try {
		YAML::Emitter out;

		out << YAML::BeginMap;

		static const QString defaults = loadDefaultCommonMacros();
		if (CommonMacros == defaults) {
			out << YAML::Key << "common" << YAML::Value << "Default";
		} else {
			out << YAML::Key << "common" << YAML::Value << YAML::Literal << CommonMacros.toUtf8().data();
		}

		out << YAML::Key << "languages" << YAML::Value;

		out << YAML::BeginMap;
		for (const SmartIndentEntry &sis : SmartIndentSpecs) {
			out << YAML::Key << sis.language.toUtf8().data() << YAML::Value;
			if (isDefaultIndentSpec(&sis)) {
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
 * @brief writeSmartIndentCommonString
 * @return
 */
QString writeSmartIndentCommonString() {
	return QLatin1String("*");
}

/**
 * @brief findIndentSpec
 * @param language
 * @return
 */
const SmartIndentEntry *findIndentSpec(const QString &language) {

	if (language.isNull()) {
		return nullptr;
	}

	auto it = std::find_if(SmartIndentSpecs.begin(), SmartIndentSpecs.end(), [&language](const SmartIndentEntry &entry) {
		return entry.language == language;
	});

	if (it != SmartIndentSpecs.end()) {
		return &*it;
	}

	return nullptr;
}

/*
** Returns true if there are smart indent macros, or potential macros
** not yet committed in the smart indent dialog for a language mode,
*/
bool LMHasSmartIndentMacros(const QString &languageMode) {
	if (findIndentSpec(languageMode) != nullptr) {
		return true;
	}

	return SmartIndentDialog && SmartIndentDialog->languageMode() == languageMode;
}

/*
** Change the language mode name of smart indent macro sets for language
** "oldName" to "newName" in both the stored macro sets, and the pattern set
** currently being edited in the dialog.
*/
void renameSmartIndentMacros(const QString &oldName, const QString &newName) {

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

/*
** If a smart indent dialog is up, ask to have the option menu for
** choosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void updateLangModeMenuSmartIndent() {

	if (SmartIndentDialog) {
		SmartIndentDialog->updateLanguageModes();
	}
}

}
