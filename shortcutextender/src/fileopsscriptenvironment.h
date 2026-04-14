/*
 * Fooyin
 * Copyright © 2026
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <core/scripting/scripttypes.h>

namespace Fooyin::ShortcutExtender {

// Mirrors FileOpsScriptEnvironment from fileopsworker.cpp.
// Enables replacePathSeparators so that ScriptParser replaces '/' and '\'
// within evaluated variable values with '-', preventing unintended extra path
// segments from track metadata (e.g. artist names containing '/').
class FileOpsScriptEnvironment : public ScriptEnvironment, public ScriptEvaluationEnvironment
{
public:
    [[nodiscard]] const ScriptEvaluationEnvironment* evaluationEnvironment() const override
    {
        return this;
    }

    [[nodiscard]] TrackListContextPolicy trackListContextPolicy() const override
    {
        return TrackListContextPolicy::Unresolved;
    }

    [[nodiscard]] QString trackListPlaceholder() const override
    {
        return {};
    }

    [[nodiscard]] bool escapeRichText() const override
    {
        return false;
    }

    [[nodiscard]] bool replacePathSeparators() const override
    {
        return true;
    }
};

} // namespace Fooyin::ShortcutExtender
