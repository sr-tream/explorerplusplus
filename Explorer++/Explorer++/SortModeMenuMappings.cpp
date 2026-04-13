// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "SortModeMenuMappings.h"
#include "MainResource.h"
#include <boost/bimap.hpp>
#include <algorithm>
#include <span>

namespace
{

using MenuItemIdToSortModePair = std::pair<UINT, SortMode>;
using SortModeBimap = boost::bimap<UINT, SortMode>;

// clang-format off
constexpr MenuItemIdToSortModePair MENU_ITEM_SORT_MODE_MAPPINGS[] = {
	{ IDM_SORTBY_NAME, SortMode::Name },
	{ IDM_SORTBY_SIZE, SortMode::Size },
	{ IDM_SORTBY_TYPE, SortMode::Type },
	{ IDM_SORTBY_DATEMODIFIED, SortMode::DateModified },
	{ IDM_SORTBY_TOTALSIZE, SortMode::TotalSize },
	{ IDM_SORTBY_FREESPACE, SortMode::FreeSpace },
	{ IDM_SORTBY_DATEDELETED, SortMode::DateDeleted },
	{ IDM_SORTBY_ORIGINALLOCATION, SortMode::OriginalLocation },
	{ IDM_SORTBY_ATTRIBUTES, SortMode::Attributes },
	{ IDM_SORTBY_REALSIZE, SortMode::RealSize },
	{ IDM_SORTBY_SHORTNAME, SortMode::ShortName },
	{ IDM_SORTBY_OWNER, SortMode::Owner },
	{ IDM_SORTBY_PRODUCTNAME, SortMode::ProductName },
	{ IDM_SORTBY_COMPANY, SortMode::Company },
	{ IDM_SORTBY_DESCRIPTION, SortMode::Description },
	{ IDM_SORTBY_FILEVERSION, SortMode::FileVersion },
	{ IDM_SORTBY_PRODUCTVERSION, SortMode::ProductVersion },
	{ IDM_SORTBY_SHORTCUTTO, SortMode::ShortcutTo },
	{ IDM_SORTBY_HARDLINKS, SortMode::HardLinks },
	{ IDM_SORTBY_EXTENSION, SortMode::Extension },
	{ IDM_SORTBY_CREATED, SortMode::Created },
	{ IDM_SORTBY_ACCESSED, SortMode::Accessed },
	{ IDM_SORTBY_TITLE, SortMode::Title },
	{ IDM_SORTBY_SUBJECT, SortMode::Subject },
	{ IDM_SORTBY_AUTHORS, SortMode::Authors },
	{ IDM_SORTBY_KEYWORDS, SortMode::Keywords },
	{ IDM_SORTBY_COMMENTS, SortMode::Comments },
	{ IDM_SORTBY_CAMERAMODEL, SortMode::CameraModel },
	{ IDM_SORTBY_DATETAKEN, SortMode::DateTaken },
	{ IDM_SORTBY_WIDTH, SortMode::Width },
	{ IDM_SORTBY_HEIGHT, SortMode::Height },
	{ IDM_SORTBY_VIRTUALCOMMENTS, SortMode::VirtualComments },
	{ IDM_SORTBY_FILESYSTEM, SortMode::FileSystem },
	{ IDM_SORTBY_NUMPRINTERDOCUMENTS, SortMode::NumPrinterDocuments },
	{ IDM_SORTBY_PRINTERSTATUS, SortMode::PrinterStatus },
	{ IDM_SORTBY_PRINTERCOMMENTS, SortMode::PrinterComments },
	{ IDM_SORTBY_PRINTERLOCATION, SortMode::PrinterLocation },
	{ IDM_SORTBY_NETWORKADAPTER_STATUS, SortMode::NetworkAdapterStatus },
	{ IDM_SORTBY_MEDIA_BITRATE, SortMode::MediaBitrate },
	{ IDM_SORTBY_MEDIA_COPYRIGHT, SortMode::MediaCopyright },
	{ IDM_SORTBY_MEDIA_DURATION, SortMode::MediaDuration },
	{ IDM_SORTBY_MEDIA_PROTECTED, SortMode::MediaProtected },
	{ IDM_SORTBY_MEDIA_RATING, SortMode::MediaRating },
	{ IDM_SORTBY_MEDIA_ALBUM_ARTIST, SortMode::MediaAlbumArtist },
	{ IDM_SORTBY_MEDIA_ALBUM, SortMode::MediaAlbum },
	{ IDM_SORTBY_MEDIA_BEATS_PER_MINUTE, SortMode::MediaBeatsPerMinute },
	{ IDM_SORTBY_MEDIA_COMPOSER, SortMode::MediaComposer },
	{ IDM_SORTBY_MEDIA_CONDUCTOR, SortMode::MediaConductor },
	{ IDM_SORTBY_MEDIA_DIRECTOR, SortMode::MediaDirector },
	{ IDM_SORTBY_MEDIA_GENRE, SortMode::MediaGenre },
	{ IDM_SORTBY_MEDIA_LANGUAGE, SortMode::MediaLanguage },
	{ IDM_SORTBY_MEDIA_BROADCAST_DATE, SortMode::MediaBroadcastDate },
	{ IDM_SORTBY_MEDIA_CHANNEL, SortMode::MediaChannel },
	{ IDM_SORTBY_MEDIA_STATION_NAME, SortMode::MediaStationName },
	{ IDM_SORTBY_MEDIA_MOOD, SortMode::MediaMood },
	{ IDM_SORTBY_MEDIA_PARENTAL_RATING, SortMode::MediaParentalRating },
	{ IDM_SORTBY_MEDIA_PARENTAL_RATING_REASON, SortMode::MediaParentalRatingReason },
	{ IDM_SORTBY_MEDIA_PERIOD, SortMode::MediaPeriod },
	{ IDM_SORTBY_MEDIA_PRODUCER, SortMode::MediaProducer },
	{ IDM_SORTBY_MEDIA_PUBLISHER, SortMode::MediaPublisher },
	{ IDM_SORTBY_MEDIA_WRITER, SortMode::MediaWriter },
	{ IDM_SORTBY_MEDIA_YEAR, SortMode::MediaYear }
};

// clang-format off
constexpr MenuItemIdToSortModePair MENU_ITEM_GROUP_MODE_MAPPINGS[] = {
	{ IDM_GROUPBY_NAME, SortMode::Name },
	{ IDM_GROUPBY_SIZE, SortMode::Size },
	{ IDM_GROUPBY_TYPE, SortMode::Type },
	{ IDM_GROUPBY_DATEMODIFIED, SortMode::DateModified },
	{ IDM_GROUPBY_TOTALSIZE, SortMode::TotalSize },
	{ IDM_GROUPBY_FREESPACE, SortMode::FreeSpace },
	{ IDM_GROUPBY_DATEDELETED, SortMode::DateDeleted },
	{ IDM_GROUPBY_ORIGINALLOCATION, SortMode::OriginalLocation },
	{ IDM_GROUPBY_ATTRIBUTES, SortMode::Attributes },
	{ IDM_GROUPBY_REALSIZE, SortMode::RealSize },
	{ IDM_GROUPBY_SHORTNAME, SortMode::ShortName },
	{ IDM_GROUPBY_OWNER, SortMode::Owner },
	{ IDM_GROUPBY_PRODUCTNAME, SortMode::ProductName },
	{ IDM_GROUPBY_COMPANY, SortMode::Company },
	{ IDM_GROUPBY_DESCRIPTION, SortMode::Description },
	{ IDM_GROUPBY_FILEVERSION, SortMode::FileVersion },
	{ IDM_GROUPBY_PRODUCTVERSION, SortMode::ProductVersion },
	{ IDM_GROUPBY_SHORTCUTTO, SortMode::ShortcutTo },
	{ IDM_GROUPBY_HARDLINKS, SortMode::HardLinks },
	{ IDM_GROUPBY_EXTENSION, SortMode::Extension },
	{ IDM_GROUPBY_CREATED, SortMode::Created },
	{ IDM_GROUPBY_ACCESSED, SortMode::Accessed },
	{ IDM_GROUPBY_TITLE, SortMode::Title },
	{ IDM_GROUPBY_SUBJECT, SortMode::Subject },
	{ IDM_GROUPBY_AUTHOR, SortMode::Authors },
	{ IDM_GROUPBY_KEYWORDS, SortMode::Keywords },
	{ IDM_GROUPBY_COMMENTS, SortMode::Comments },
	{ IDM_GROUPBY_CAMERAMODEL, SortMode::CameraModel },
	{ IDM_GROUPBY_DATETAKEN, SortMode::DateTaken },
	{ IDM_GROUPBY_WIDTH, SortMode::Width },
	{ IDM_GROUPBY_HEIGHT, SortMode::Height },
	{ IDM_GROUPBY_VIRTUALCOMMENTS, SortMode::VirtualComments },
	{ IDM_GROUPBY_FILESYSTEM, SortMode::FileSystem },
	{ IDM_GROUPBY_NUMPRINTERDOCUMENTS, SortMode::NumPrinterDocuments },
	{ IDM_GROUPBY_PRINTERSTATUS, SortMode::PrinterStatus },
	{ IDM_GROUPBY_PRINTERCOMMENTS, SortMode::PrinterComments },
	{ IDM_GROUPBY_PRINTERLOCATION, SortMode::PrinterLocation },
	{ IDM_GROUPBY_NETWORKADAPTER_STATUS, SortMode::NetworkAdapterStatus },
	{ IDM_GROUPBY_MEDIA_BITRATE, SortMode::MediaBitrate },
	{ IDM_GROUPBY_MEDIA_COPYRIGHT, SortMode::MediaCopyright },
	{ IDM_GROUPBY_MEDIA_DURATION, SortMode::MediaDuration },
	{ IDM_GROUPBY_MEDIA_PROTECTED, SortMode::MediaProtected },
	{ IDM_GROUPBY_MEDIA_RATING, SortMode::MediaRating },
	{ IDM_GROUPBY_MEDIA_ALBUM_ARTIST, SortMode::MediaAlbumArtist },
	{ IDM_GROUPBY_MEDIA_ALBUM, SortMode::MediaAlbum },
	{ IDM_GROUPBY_MEDIA_BEATS_PER_MINUTE, SortMode::MediaBeatsPerMinute },
	{ IDM_GROUPBY_MEDIA_COMPOSER, SortMode::MediaComposer },
	{ IDM_GROUPBY_MEDIA_CONDUCTOR, SortMode::MediaConductor },
	{ IDM_GROUPBY_MEDIA_DIRECTOR, SortMode::MediaDirector },
	{ IDM_GROUPBY_MEDIA_GENRE, SortMode::MediaGenre },
	{ IDM_GROUPBY_MEDIA_LANGUAGE, SortMode::MediaLanguage },
	{ IDM_GROUPBY_MEDIA_BROADCAST_DATE, SortMode::MediaBroadcastDate },
	{ IDM_GROUPBY_MEDIA_CHANNEL, SortMode::MediaChannel },
	{ IDM_GROUPBY_MEDIA_STATION_NAME, SortMode::MediaStationName },
	{ IDM_GROUPBY_MEDIA_MOOD, SortMode::MediaMood },
	{ IDM_GROUPBY_MEDIA_PARENTAL_RATING, SortMode::MediaParentalRating },
	{ IDM_GROUPBY_MEDIA_PARENTAL_RATING_REASON, SortMode::MediaParentalRatingReason },
	{ IDM_GROUPBY_MEDIA_PERIOD, SortMode::MediaPeriod },
	{ IDM_GROUPBY_MEDIA_PRODUCER, SortMode::MediaProducer },
	{ IDM_GROUPBY_MEDIA_PUBLISHER, SortMode::MediaPublisher },
	{ IDM_GROUPBY_MEDIA_WRITER, SortMode::MediaWriter },
	{ IDM_GROUPBY_MEDIA_YEAR, SortMode::MediaYear }
};
// clang-format on
// clang-format on

constexpr bool VerifyMappings(std::span<const MenuItemIdToSortModePair> mappings)
{
	if (mappings.size() != SortMode::_size())
	{
		return false;
	}

	// Each sort mode should appear.
	for (auto sortMode : SortMode::_values())
	{
		auto itr = std::ranges::find_if(mappings,
			[sortMode](const auto &mapping) { return mapping.second == sortMode; });

		if (itr == mappings.end())
		{
			return false;
		}
	}

	// Each menu item should appear once.
	for (const auto &mapping : mappings)
	{
		auto numMatches = std::ranges::count_if(mappings, [&mapping](const auto &currentMapping)
			{ return currentMapping.first == mapping.first; });

		if (numMatches > 1)
		{
			return false;
		}
	}

	return true;
}

static_assert(VerifyMappings(MENU_ITEM_SORT_MODE_MAPPINGS));
static_assert(VerifyMappings(MENU_ITEM_GROUP_MODE_MAPPINGS));

const SortModeBimap &GetSortModeBimap()
{
	static const SortModeBimap map = []()
	{
		SortModeBimap innerMap;

		for (const auto &mapping : MENU_ITEM_SORT_MODE_MAPPINGS)
		{
			auto [itr, didInsert] = innerMap.insert({ mapping.first, mapping.second });
			DCHECK(didInsert);
		}

		return innerMap;
	}();
	return map;
}

const SortModeBimap &GetGroupModeBimap()
{
	static const SortModeBimap map = []()
	{
		SortModeBimap innerMap;

		for (const auto &mapping : MENU_ITEM_GROUP_MODE_MAPPINGS)
		{
			auto [itr, didInsert] = innerMap.insert({ mapping.first, mapping.second });
			DCHECK(didInsert);
		}

		return innerMap;
	}();
	return map;
}

}

bool IsSortModeMenuItemId(UINT menuItemId)
{
	const auto &map = GetSortModeBimap();
	auto itr = map.left.find(menuItemId);
	return itr != map.left.end();
}

SortMode GetSortModeForMenuItemId(UINT menuItemId)
{
	const auto &map = GetSortModeBimap();
	auto itr = map.left.find(menuItemId);
	CHECK(itr != map.left.end());
	return itr->second;
}

UINT GetMenuItemIdForSortMode(SortMode sortMode)
{
	const auto &map = GetSortModeBimap();
	auto itr = map.right.find(sortMode);
	CHECK(itr != map.right.end());
	return itr->second;
}

bool IsGroupModeMenuItemId(UINT menuItemId)
{
	const auto &map = GetGroupModeBimap();
	auto itr = map.left.find(menuItemId);
	return itr != map.left.end();
}

SortMode GetSortModeForGroupMenuItemId(UINT menuItemId)
{
	const auto &map = GetGroupModeBimap();
	auto itr = map.left.find(menuItemId);
	CHECK(itr != map.left.end());
	return itr->second;
}

UINT GetGroupMenuItemIdForSortMode(SortMode sortMode)
{
	const auto &map = GetGroupModeBimap();
	auto itr = map.right.find(sortMode);
	CHECK(itr != map.right.end());
	return itr->second;
}
