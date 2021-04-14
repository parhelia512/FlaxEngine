// Copyright (c) 2012-2021 Wojciech Figat. All rights reserved.

#pragma once

#include "CultureInfo.h"
#include "Engine/Core/Types/BaseTypes.h"

/// <summary>
/// The language and culture localization manager.
/// </summary>
API_CLASS(Static) class FLAXENGINE_API Localization
{
DECLARE_SCRIPTING_TYPE_MINIMAL(Localization);
public:
    /// <summary>
    /// Gets the current culture (date, time, currency and values formatting locale).
    /// </summary>
    API_PROPERTY() static const CultureInfo& GetCurrentCulture();

    /// <summary>
    /// Sets the current culture (date, time, currency and values formatting locale).
    /// </summary>
    API_PROPERTY() static void SetCurrentCulture(const CultureInfo& value);

    /// <summary>
    /// Gets the current language (text display locale).
    /// </summary>
    API_PROPERTY() static const CultureInfo& GetCurrentLanguage();

    /// <summary>
    /// Sets the current language (text display locale).
    /// </summary>
    API_PROPERTY() static void SetCurrentLanguage(const CultureInfo& value);

    /// <summary>
    /// Sets both the current language (text display locale) and the current culture (date, time, currency and values formatting locale) at once.
    /// </summary>
    API_FUNCTION() static void SetCurrentLanguageCulture(const CultureInfo& value);

    /// <summary>
    /// Occurs when current culture or language gets changed. Can be used to refresh UI to reflect language changes.
    /// </summary>
    API_EVENT() static Delegate<> CurrentLanguageCultureChanged;
};
