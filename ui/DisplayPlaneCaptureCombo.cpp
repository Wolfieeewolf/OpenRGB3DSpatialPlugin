// SPDX-License-Identifier: GPL-2.0-only

#include "DisplayPlaneCaptureCombo.h"

#include "DisplayPlane3D.h"
#include "ScreenCaptureManager.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QString>
#include <QVariant>

void FillDisplayPlaneCaptureCombo(QComboBox* combo,
                                  const std::string& prefer_source_id,
                                  const std::vector<std::string>& used_ids,
                                  CaptureComboEmptyPolicy empty_policy)
{
    if(!combo)
    {
        return;
    }

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }
    capture_mgr.RefreshSources();
    std::vector<CaptureSourceInfo> sources = capture_mgr.GetAvailableSources();
    SortCaptureSourcesByDisplayNumber(sources);

    QSignalBlocker block(combo);
    combo->clear();
    combo->addItem(QStringLiteral("(None)"), QString());

    for(const CaptureSourceInfo& source : sources)
    {
        combo->addItem(QString::fromStdString(FormatCaptureSourceLabel(source)),
                       QString::fromStdString(source.id));
    }

    std::string select_id = prefer_source_id;
    if(select_id.empty() && empty_policy == CaptureComboEmptyPolicy::SelectSuggested)
    {
        select_id = SuggestCaptureSourceId(sources, used_ids);
    }
    else if(select_id.empty())
    {
        switch(empty_policy)
        {
            case CaptureComboEmptyPolicy::SelectNone:
            case CaptureComboEmptyPolicy::SelectSuggested:
                break;
        }
    }

    if(!select_id.empty())
    {
        const QString select_q = QString::fromStdString(select_id);
        for(int i = 0; i < combo->count(); i++)
        {
            if(combo->itemData(i).toString() == select_q)
            {
                combo->setCurrentIndex(i);
                return;
            }
        }
        combo->addItem(select_q + QStringLiteral(" (unavailable)"), select_q);
        combo->setCurrentIndex(combo->count() - 1);
        return;
    }

    combo->setCurrentIndex(0);
}

std::string CaptureComboSourceId(const QComboBox* combo)
{
    if(!combo)
    {
        return {};
    }
    const int index = combo->currentIndex();
    if(index < 0)
    {
        return {};
    }
    return combo->itemData(index).toString().toStdString();
}

std::string CaptureComboSourceLabel(const QComboBox* combo)
{
    if(!combo)
    {
        return {};
    }
    const int index = combo->currentIndex();
    if(index < 0)
    {
        return {};
    }
    if(combo->itemData(index).toString().isEmpty())
    {
        return {};
    }
    return combo->currentText().toStdString();
}

void ApplyCaptureComboToPlane(DisplayPlane3D* plane, const QComboBox* combo)
{
    if(!plane)
    {
        return;
    }
    plane->SetCaptureSourceId(CaptureComboSourceId(combo));
    plane->SetCaptureLabel(CaptureComboSourceLabel(combo));
}
