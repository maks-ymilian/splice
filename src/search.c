#include "search.h"

#include <stdlib.h>

#include <cJSON.h>

#include <curl/curl.h>

#include "common.h"
#include "platform.h"

static const char* search_graphql_query = "query SamplesSearch($parent_asset_uuid: GUID, $query: String, $order: SortOrder = DESC, $sort: AssetSortType = popularity, $random_seed: String, $tags: [ID], $key: String, $chord_type: String, $bpm: String, $min_bpm: Int, $max_bpm: Int, $limit: Int = 50, $asset_category_slug: AssetCategorySlug, $page: Int = 1, $ac_uuid: String, $parent_asset_type: AssetTypeSlug) {\n  assetsSearch(\n    filter: {legacy: true, published: true, asset_type_slug: sample, query: $query, tag_ids: $tags, key: $key, chord_type: $chord_type, bpm: $bpm, min_bpm: $min_bpm, max_bpm: $max_bpm, asset_category_slug: $asset_category_slug, ac_uuid: $ac_uuid}\n    children: {parent_asset_uuid: $parent_asset_uuid}\n    pagination: {page: $page, limit: $limit}\n    sort: {sort: $sort, order: $order, random_seed: $random_seed}\n    legacy: {parent_asset_type: $parent_asset_type}\n  ) {\n    ...assetDetails\n    __typename\n  }\n}\n\nfragment assetDetails on AssetPage {\n  ...assetPageItems\n  ...assetTagSummaries\n  pagination_metadata {\n    currentPage\n    totalPages\n    __typename\n  }\n  response_metadata {\n    records\n    __typename\n  }\n  __typename\n}\n\nfragment assetPageItems on AssetPage {\n  items {\n    ... on IAsset {\n      asset_type_slug\n      asset_prices {\n        amount\n        currency\n        __typename\n      }\n      uuid\n      name\n      tags {\n        uuid\n        label\n        __typename\n      }\n      files {\n        uuid\n        name\n        hash\n        path\n        asset_file_type_slug\n        url\n        __typename\n      }\n      __typename\n    }\n    ... on IAssetChild {\n      parents(filter: {asset_type_slug: pack}) {\n        items {\n          ... on PackAsset {\n            permalink_slug\n            permalink_base_url\n            uuid\n            name\n            files {\n              uuid\n              path\n              asset_file_type_slug\n              url\n              __typename\n            }\n            __typename\n          }\n          __typename\n        }\n        __typename\n      }\n      __typename\n    }\n    ... on SampleAsset {\n      bpm\n      chord_type\n      key\n      duration\n      uuid\n      name\n      asset_category_slug\n      __typename\n    }\n    ... on PresetAsset {\n      uuid\n      name\n      asset_devices {\n        uuid\n        device {\n          name\n          uuid\n          minimum_device_version\n          __typename\n        }\n        __typename\n      }\n      __typename\n    }\n    ... on PackAsset {\n      uuid\n      name\n      provider {\n        name\n        permalink_slug\n        __typename\n      }\n      provider_uuid\n      uuid\n      permalink_slug\n      permalink_base_url\n      main_genre\n      __typename\n    }\n    __typename\n  }\n  __typename\n}\n\nfragment assetTagSummaries on AssetPage {\n  tag_summary {\n    count\n    tag {\n      uuid\n      label\n      taxonomy {\n        uuid\n        name\n        __typename\n      }\n      __typename\n    }\n    __typename\n  }\n  __typename\n}";

void free_search_results(struct search_results results)
{
	if (results.ptr)
	{
		for (int i = 0; i < results.length; ++i)
		{
			free(results.ptr[i].name);
			free(results.ptr[i].name_full);
			free(results.ptr[i].sound_url);
		}
	}

	free(results.ptr);
}

static cJSON* build_search_body(char* query)
{
	cJSON* json = cJSON_CreateObject();

	if (!cJSON_AddStringToObject(json, "operationName", "SamplesSearch"))
		goto error;

	cJSON* variables = cJSON_AddObjectToObject(json, "variables");
	if (!variables)
		goto error;

	{
		if (!cJSON_AddStringToObject(variables, "order", "DESC"))
			goto error;

		if (!cJSON_AddStringToObject(variables, "sort", "popularity"))
			goto error;

		if (!cJSON_AddNumberToObject(variables, "limit", 20))
			goto error;

		if (!cJSON_AddNumberToObject(variables, "page", 1))
			goto error;

		if (!cJSON_AddArrayToObject(variables, "tags"))
			goto error;

		if (!cJSON_AddNullToObject(variables, "key"))
			goto error;

		if (!cJSON_AddNullToObject(variables, "chord_type"))
			goto error;

		if (!cJSON_AddNullToObject(variables, "bpm"))
			goto error;

		if (!cJSON_AddNullToObject(variables, "min_bpm"))
			goto error;

		if (!cJSON_AddNullToObject(variables, "max_bpm"))
			goto error;

		if (!cJSON_AddNullToObject(variables, "asset_category_slug"))
			goto error;

		if (!cJSON_AddNullToObject(variables, "random_seed"))
			goto error;

		if (!cJSON_AddNullToObject(variables, "random_seed"))
			goto error;

		if (!cJSON_AddStringToObject(variables, "query", query))
			goto error;

		if (!cJSON_AddNullToObject(variables, "ac_uuid"))
			goto error;
	}

	if (!cJSON_AddStringToObject(json, "query", search_graphql_query))
		goto error;

	return json;

error:
	cJSON_Delete(json);
	return NULL;
}

bool search(char* text, struct search_results* results)
{
	CURL* list_request = curl_easy_init();
	struct buffer response = {0};
	struct curl_slist* headers = NULL;
	cJSON* response_json = NULL;
	cJSON* search_body_json = NULL;
	char* search_body = NULL;
	bool return_value = false;

	headers = curl_slist_append(headers, "content-type: application/json");

	search_body_json = build_search_body(text);
	if (!search_body_json)
		goto cleanup;

	search_body = cJSON_Print(search_body_json);
	if (!search_body)
		goto cleanup;

	curl_easy_setopt(list_request, CURLOPT_URL, "https://surfaces-graphql.splice.com/graphql");
	curl_easy_setopt(list_request, CURLOPT_POST, 1L);
	curl_easy_setopt(list_request, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(list_request, CURLOPT_POSTFIELDS, search_body);
	curl_easy_setopt(list_request, CURLOPT_WRITEFUNCTION, write_response);
	curl_easy_setopt(list_request, CURLOPT_WRITEDATA, &response);

	if (curl_easy_perform(list_request))
		goto cleanup;

	// long status_code;
	// curl_easy_getinfo(list_request, CURLINFO_RESPONSE_CODE, &status_code);
	// printf("status code: %ld\n", status_code);

	response_json = cJSON_ParseWithLength(response.data, response.length);

	if (!cJSON_IsObject(response_json))
		goto cleanup;
	cJSON* data = cJSON_GetObjectItemCaseSensitive(response_json, "data");
	if (!cJSON_IsObject(data))
		goto cleanup;
	cJSON* assets_search = cJSON_GetObjectItemCaseSensitive(data, "assetsSearch");
	if (!cJSON_IsObject(assets_search))
		goto cleanup;
	cJSON* items = cJSON_GetObjectItemCaseSensitive(assets_search, "items");
	if (!cJSON_IsArray(items))
		goto cleanup;

	*results = (struct search_results){0};
	
	for (int i = 0; i < cJSON_GetArraySize(items); ++i)
	{
		cJSON* item = cJSON_GetArrayItem(items, i);
		if (!cJSON_IsObject(item))
			goto cleanup;

		cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
		if (!cJSON_IsString(name))
			goto cleanup;

		cJSON* files = cJSON_GetObjectItemCaseSensitive(item, "files");
		if (!cJSON_IsArray(files) || cJSON_GetArraySize(files) < 1)
			goto cleanup;

		cJSON* audio = cJSON_GetArrayItem(files, 0);
		if (!cJSON_IsObject(audio))
			goto cleanup;

		cJSON* url = cJSON_GetObjectItemCaseSensitive(audio, "url");
		if (!cJSON_IsString(url))
			goto cleanup;

		++results->length;
		results->ptr = realloc(results->ptr, sizeof(*results->ptr) * results->length);
		results->ptr[i] = (struct search_result){
			.name = alloc_file_name(name->valuestring),
			.name_full = realloc_string(name->valuestring),
			.sound_url = realloc_string(url->valuestring),
		};
	}

	return_value = true;

cleanup:
	free(response.data);
	free(search_body);
	cJSON_Delete(response_json);
	cJSON_Delete(search_body_json);
	curl_slist_free_all(headers);
	curl_easy_cleanup(list_request);

	return return_value;
}
