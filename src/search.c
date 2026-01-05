#include "search.h"

#include <stdlib.h>

#include <cJSON.h>

#include <curl/curl.h>

#include "common.h"
#include "file_utils.h"

struct search_context
{
	struct database* db;
	search_item_update_func item_update_func;
	int max_results_per_page;
};

static const char* search_graphql_query = "query SamplesSearch($parent_asset_uuid: GUID, $query: String, $order: SortOrder = DESC, $sort: AssetSortType = popularity, $random_seed: String, $tags: [ID], $key: String, $chord_type: String, $bpm: String, $min_bpm: Int, $max_bpm: Int, $limit: Int = 50, $asset_category_slug: AssetCategorySlug, $page: Int = 1, $ac_uuid: String, $parent_asset_type: AssetTypeSlug) {\n  assetsSearch(\n    filter: {legacy: true, published: true, asset_type_slug: sample, query: $query, tag_ids: $tags, key: $key, chord_type: $chord_type, bpm: $bpm, min_bpm: $min_bpm, max_bpm: $max_bpm, asset_category_slug: $asset_category_slug, ac_uuid: $ac_uuid}\n    children: {parent_asset_uuid: $parent_asset_uuid}\n    pagination: {page: $page, limit: $limit}\n    sort: {sort: $sort, order: $order, random_seed: $random_seed}\n    legacy: {parent_asset_type: $parent_asset_type}\n  ) {\n    ...assetDetails\n    __typename\n  }\n}\n\nfragment assetDetails on AssetPage {\n  ...assetPageItems\n  ...assetTagSummaries\n  pagination_metadata {\n    currentPage\n    totalPages\n    __typename\n  }\n  response_metadata {\n    records\n    __typename\n  }\n  __typename\n}\n\nfragment assetPageItems on AssetPage {\n  items {\n    ... on IAsset {\n      asset_type_slug\n      asset_prices {\n        amount\n        currency\n        __typename\n      }\n      uuid\n      name\n      tags {\n        uuid\n        label\n        __typename\n      }\n      files {\n        uuid\n        name\n        hash\n        path\n        asset_file_type_slug\n        url\n        __typename\n      }\n      __typename\n    }\n    ... on IAssetChild {\n      parents(filter: {asset_type_slug: pack}) {\n        items {\n          ... on PackAsset {\n            permalink_slug\n            permalink_base_url\n            uuid\n            name\n            files {\n              uuid\n              path\n              asset_file_type_slug\n              url\n              __typename\n            }\n            __typename\n          }\n          __typename\n        }\n        __typename\n      }\n      __typename\n    }\n    ... on SampleAsset {\n      bpm\n      chord_type\n      key\n      duration\n      uuid\n      name\n      asset_category_slug\n      __typename\n    }\n    ... on PresetAsset {\n      uuid\n      name\n      asset_devices {\n        uuid\n        device {\n          name\n          uuid\n          minimum_device_version\n          __typename\n        }\n        __typename\n      }\n      __typename\n    }\n    ... on PackAsset {\n      uuid\n      name\n      provider {\n        name\n        permalink_slug\n        __typename\n      }\n      provider_uuid\n      uuid\n      permalink_slug\n      permalink_base_url\n      main_genre\n      __typename\n    }\n    __typename\n  }\n  __typename\n}\n\nfragment assetTagSummaries on AssetPage {\n  tag_summary {\n    count\n    tag {\n      uuid\n      label\n      taxonomy {\n        uuid\n        name\n        __typename\n      }\n      __typename\n    }\n    __typename\n  }\n  __typename\n}";

static cJSON* build_search_body(struct search_query query, int max_results_per_page, int page)
{
	if (!query.search_string || max_results_per_page < 1 || page < 1) return NULL;

	cJSON* json = NULL;
	if (!(json = cJSON_CreateObject())) return NULL;

	if (!cJSON_AddStringToObject(json, "operationName", "SamplesSearch")) goto error;

	cJSON* variables = NULL;
	if ((variables = cJSON_AddObjectToObject(json, "variables")))
	{
		if (query.used_parameters & SEARCH_QUERY_PARAMETERS_TAGS)
		{
			if (!cJSON_AddArrayToObject(variables, "tags")) goto error;
		}
		if (query.used_parameters & SEARCH_QUERY_PARAMETERS_BPM)
		{
			if (query.bpm_range.min == query.bpm_range.max)
			{
				char text[10];
				if (snprintf(text, COUNTOF(text), "%d", query.bpm_range.min) < 1) goto error;
				if (!cJSON_AddStringToObject(variables, "bpm", text)) goto error;
			}
			else
			{
				if (!cJSON_AddNumberToObject(variables, "min_bpm", query.bpm_range.min)) goto error;
				if (!cJSON_AddNumberToObject(variables, "max_bpm", query.bpm_range.max)) goto error;
			}
		}
		if (query.used_parameters & SEARCH_QUERY_PARAMETERS_SORT)
		{
			if (query.sort == SEARCH_SORT_MOST_POPULAR) { if (!cJSON_AddStringToObject(variables, "sort", "popularity")) goto error; }
			else if (query.sort == SEARCH_SORT_MOST_RELEVANT) { if (!cJSON_AddStringToObject(variables, "sort", "relevance")) goto error; }
			else if (query.sort == SEARCH_SORT_MOST_RECENT) { if (!cJSON_AddStringToObject(variables, "sort", "recency")) goto error; }
			else if (query.sort == SEARCH_SORT_RANDOM)
			{
				int64_t seed = 1000000000LL + (int64_t)rand();
				char seed_text[32];
				if (snprintf(seed_text, COUNTOF(seed_text), "%" PRIi64, seed) < 1) goto error;

				if (!cJSON_AddStringToObject(variables, "sort", "random")) goto error;
				if (!cJSON_AddStringToObject(variables, "random_seed", seed_text)) goto error;
			}

			if (!cJSON_AddStringToObject(variables, "order", "DESC")) goto error;
		}
		if (query.used_parameters & SEARCH_QUERY_PARAMETERS_SAMPLE_TYPE)
		{
			if (query.sample_type == SEARCH_SAMPLE_TYPE_LOOPS)          { if (!cJSON_AddStringToObject(variables, "asset_category_slug", "loop")) goto error; }
			else if (query.sample_type == SEARCH_SAMPLE_TYPE_ONE_SHOTS) { if (!cJSON_AddStringToObject(variables, "asset_category_slug", "oneshot")) goto error; }
		}
		if (query.used_parameters & SEARCH_QUERY_PARAMETERS_KEY)
		{
			if (query.key == SEARCH_KEY_C)            { if (!cJSON_AddStringToObject(variables, "key", "c")) goto error; }
			else if (query.key == SEARCH_KEY_C_SHARP) { if (!cJSON_AddStringToObject(variables, "key", "c#")) goto error; }
			else if (query.key == SEARCH_KEY_D)       { if (!cJSON_AddStringToObject(variables, "key", "d")) goto error; }
			else if (query.key == SEARCH_KEY_D_SHARP) { if (!cJSON_AddStringToObject(variables, "key", "d#")) goto error; }
			else if (query.key == SEARCH_KEY_E)       { if (!cJSON_AddStringToObject(variables, "key", "e")) goto error; }
			else if (query.key == SEARCH_KEY_F)       { if (!cJSON_AddStringToObject(variables, "key", "f")) goto error; }
			else if (query.key == SEARCH_KEY_F_SHARP) { if (!cJSON_AddStringToObject(variables, "key", "f#")) goto error; }
			else if (query.key == SEARCH_KEY_G)       { if (!cJSON_AddStringToObject(variables, "key", "g")) goto error; }
			else if (query.key == SEARCH_KEY_G_SHARP) { if (!cJSON_AddStringToObject(variables, "key", "g#")) goto error; }
			else if (query.key == SEARCH_KEY_A)       { if (!cJSON_AddStringToObject(variables, "key", "a")) goto error; }
			else if (query.key == SEARCH_KEY_A_SHARP) { if (!cJSON_AddStringToObject(variables, "key", "a#")) goto error; }
			else if (query.key == SEARCH_KEY_B)       { if (!cJSON_AddStringToObject(variables, "key", "b")) goto error; }
		}
		if (query.used_parameters & SEARCH_QUERY_PARAMETERS_SCALE)
		{
			if (query.scale & SEARCH_SCALE_MAJOR)      { if (!cJSON_AddStringToObject(variables, "chord_type", "major")) goto error; }
			else if (query.scale & SEARCH_SCALE_MINOR) { if (!cJSON_AddStringToObject(variables, "chord_type", "minor")) goto error; }
		}

		if (!cJSON_AddNumberToObject(variables, "limit", max_results_per_page)) goto error;
		if (!cJSON_AddNumberToObject(variables, "page", page)) goto error;
		if (!cJSON_AddStringToObject(variables, "query", query.search_string)) goto error;

		if (!cJSON_GetObjectItemCaseSensitive(variables, "tags") && !cJSON_AddArrayToObject(variables, "tags")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "bpm") && !cJSON_AddNullToObject(variables, "bpm")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "min_bpm") && !cJSON_AddNullToObject(variables, "min_bpm")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "max_bpm") && !cJSON_AddNullToObject(variables, "max_bpm")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "sort") && !cJSON_AddStringToObject(variables, "sort", "popularity")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "random_seed") && !cJSON_AddNullToObject(variables, "random_seed")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "order") && !cJSON_AddStringToObject(variables, "order", "DESC")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "asset_category_slug") && !cJSON_AddNullToObject(variables, "asset_category_slug")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "key") && !cJSON_AddNullToObject(variables, "key")) goto error;
		if (!cJSON_GetObjectItemCaseSensitive(variables, "chord_type") && !cJSON_AddNullToObject(variables, "chord_type")) goto error;

		if (!cJSON_AddNullToObject(variables, "ac_uuid")) goto error;
	}
	else
		goto error;

	if (!cJSON_AddStringToObject(json, "query", search_graphql_query)) goto error;

	return json;

error:
	cJSON_Delete(json);
	return NULL;
}

struct search_context* search_context_init(struct database* db, search_item_update_func item_update_func, int max_results_per_page)
{
	if (curl_global_init(CURL_GLOBAL_ALL)) return NULL;

	if (max_results_per_page < 1) goto error;

	struct search_context* search_context = NULL;
	if (!(search_context = calloc(1, sizeof(*search_context)))) goto error;

	*search_context = (struct search_context){
		.db = db,
		.item_update_func = item_update_func,
		.max_results_per_page = max_results_per_page,
	};

	srand(time(NULL));
	return search_context;

error:
	curl_global_cleanup();
	return NULL;
}

void search_context_uninit(struct search_context* context)
{
	free(context);

	curl_global_cleanup();
}

struct search_session* search_session_init(struct search_context* context, struct search_query query)
{
	if (!context) return NULL;

	struct search_session* search_session = NULL;
	struct search_query new_query = {0};
	if (!query.search_string) return NULL;
	new_query.search_string = query.search_string;
	new_query.used_parameters = query.used_parameters;
	if (query.used_parameters & SEARCH_QUERY_PARAMETERS_TAGS)
	{
		if (!(new_query.tags.tags = malloc(query.tags.length * sizeof(*query.tags.tags)))) goto error;
		for (int i = 0; i < query.tags.length; ++i)
			if (!(new_query.tags.tags[i] = string_alloc(query.tags.tags[i], string_length(query.tags.tags[i])))) goto error;
	}
	if (query.used_parameters & SEARCH_QUERY_PARAMETERS_BPM)         new_query.bpm_range = query.bpm_range;
	if (query.used_parameters & SEARCH_QUERY_PARAMETERS_SORT)        new_query.sort = query.sort;
	if (query.used_parameters & SEARCH_QUERY_PARAMETERS_SAMPLE_TYPE) new_query.sample_type = query.sample_type;
	if (query.used_parameters & SEARCH_QUERY_PARAMETERS_KEY)         new_query.key = query.key;
	if (query.used_parameters & SEARCH_QUERY_PARAMETERS_SCALE)       new_query.scale = query.scale;

	if (!(search_session = calloc(1, sizeof(*search_session)))) goto error;
	*search_session = (struct search_session){
		.query = new_query,
		.context = context,
	};

	return search_session;

error:
	if (new_query.tags.tags)
	{
		for (int i = 0; i < new_query.tags.length; ++i)
			free(new_query.tags.tags[i]);
		free(new_query.tags.tags);
	}
	if (search_session)
	{
		free(search_session->query.search_string);
		free(search_session);
	}
	return NULL;
}

void search_session_uninit(struct search_session* session)
{
	if (!session) return;

	if (session->pages)
	{
		for (int i = 0; i < session->pages_length; ++i)
		{
			struct search_page page = session->pages[i];
			if (page.items)
			{
				for (int i = 0; i < page.items_length; ++i)
				{
					struct search_item item = page.items[i];
					free(item.data.name);
					free(item.data.name_full);
					free(item.data.audio_url);
					free(item.data.file_on_disk);
					free(item.data.key);
					free(item.data.chord_type);

					for (int i = 0; i < item.data.tags_length; ++i)
						free(item.data.tags[i]);
					free(item.data.tags);
				}
				free(page.items);
			}
		}
		free(session->pages);
	}

	free(session);
}

bool search_session_fetch_next_page(struct search_session* session)
{
	if (!session) return false;

	++session->pages_length;
	session->pages = realloc(session->pages, session->pages_length * sizeof(*session->pages));
	struct search_page* curr_page = &session->pages[session->pages_length - 1];
	*curr_page = (struct search_page){0};

	CURL* list_request = NULL;
	struct buffer response = {0};
	struct curl_slist* headers = NULL;
	cJSON* response_json = NULL;
	cJSON* search_body_json = NULL;
	char* search_body = NULL;
	bool success = false;

	list_request = curl_easy_init();
	if (!list_request) goto cleanup;

	headers = curl_slist_append(headers, "content-type: application/json");
	if (!headers) goto cleanup;

	search_body_json = build_search_body(session->query, session->context->max_results_per_page, session->pages_length);
	if (!search_body_json) goto cleanup;

	search_body = cJSON_Print(search_body_json);
	if (!search_body) goto cleanup;

	curl_easy_setopt(list_request, CURLOPT_URL, "https://surfaces-graphql.splice.com/graphql");
	curl_easy_setopt(list_request, CURLOPT_POST, 1L);
	curl_easy_setopt(list_request, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(list_request, CURLOPT_POSTFIELDS, search_body);
	curl_easy_setopt(list_request, CURLOPT_WRITEFUNCTION, write_response);
	curl_easy_setopt(list_request, CURLOPT_WRITEDATA, &response);

	if (curl_easy_perform(list_request)) goto cleanup;

	response_json = cJSON_ParseWithLength(response.data, response.length);
	if (!cJSON_IsObject(response_json)) goto cleanup;

	cJSON* errors = cJSON_GetObjectItemCaseSensitive(response_json, "errors");
	if (errors)
	{
		char* json = cJSON_Print(response_json);
		printf("%s", json);
		free(json);
		goto cleanup;
	}

	cJSON* data = cJSON_GetObjectItemCaseSensitive(response_json, "data");
	if (!cJSON_IsObject(data)) goto cleanup;
	cJSON* assets_search = cJSON_GetObjectItemCaseSensitive(data, "assetsSearch");
	if (!cJSON_IsObject(assets_search)) goto cleanup;

	cJSON* items = cJSON_GetObjectItemCaseSensitive(assets_search, "items");
	if (!cJSON_IsArray(items)) goto cleanup;

	cJSON* pagination_metadata = cJSON_GetObjectItemCaseSensitive(assets_search, "pagination_metadata");
	if (!cJSON_IsObject(pagination_metadata)) goto cleanup;
	cJSON* total_pages = cJSON_GetObjectItemCaseSensitive(pagination_metadata, "totalPages");
	if (!cJSON_IsNumber(total_pages)) goto cleanup;
	session->total_pages = total_pages->valueint;

	// char* file_name = "response.json";
	// char* response_string = cJSON_Print(response_json);
	// struct buffer buffer = {
	// 	.data = response_string,
	// 	.length = string_length(response_string),
	// };
	// write_file(file_name, string_length(file_name), buffer, true);

	curr_page->items_length = cJSON_GetArraySize(items);
	curr_page->items = malloc(curr_page->items_length * sizeof(*curr_page->items));
	for (int i = 0; i < curr_page->items_length; ++i)
	{
		cJSON* item_json = cJSON_GetArrayItem(items, i);
		if (!cJSON_IsObject(item_json)) goto cleanup;

		cJSON* name_json = cJSON_GetObjectItemCaseSensitive(item_json, "name");
		if (!cJSON_IsString(name_json)) goto cleanup;
		cJSON* bpm_json = cJSON_GetObjectItemCaseSensitive(item_json, "bpm");
		cJSON* key_json = cJSON_GetObjectItemCaseSensitive(item_json, "key");
		cJSON* chord_type_json = cJSON_GetObjectItemCaseSensitive(item_json, "chord_type");
		cJSON* duration_json = cJSON_GetObjectItemCaseSensitive(item_json, "duration");
		cJSON* tags_json = cJSON_GetObjectItemCaseSensitive(item_json, "tags");

		cJSON* files_json = cJSON_GetObjectItemCaseSensitive(item_json, "files");
		if (!cJSON_IsArray(files_json) || cJSON_GetArraySize(files_json) < 1) goto cleanup;
		cJSON* audio_json = cJSON_GetArrayItem(files_json, 0);
		if (!cJSON_IsObject(audio_json)) goto cleanup;
		cJSON* url_json = cJSON_GetObjectItemCaseSensitive(audio_json, "url");
		if (!cJSON_IsString(url_json)) goto cleanup;

		char** tags = NULL;
		int tags_length = 0;
		if (cJSON_IsArray(tags_json))
		{
			tags_length = cJSON_GetArraySize(tags_json);
			tags = malloc(tags_length * sizeof(*tags));
			for (int i = 0; i < tags_length; ++i)
			{
				cJSON* item = cJSON_GetArrayItem(tags_json, i);
				if (!cJSON_IsObject(item)) goto exit_tags;
				cJSON* label_json = cJSON_GetObjectItemCaseSensitive(item, "label");
				if (!cJSON_IsString(label_json)) goto exit_tags;

				tags[i] = string_alloc(label_json->valuestring, string_length(label_json->valuestring));
				if (!tags[i]) goto exit_tags;
				continue;
exit_tags:
				tags_length = 0;
			}
		}

		bool has_file_name;
		file_string short_name;
		int short_name_length;
		if (!extract_file_name(name_json->valuestring, string_length(name_json->valuestring), true, &has_file_name, short_name, &short_name_length)) goto cleanup;
		char* name = string_alloc(short_name, short_name_length);
		char* name_full = string_alloc(name_json->valuestring, string_length(name_json->valuestring));
		char* audio_url = string_alloc(url_json->valuestring, string_length(url_json->valuestring));
		if (!name || !name_full || !audio_url) goto cleanup;

		int bpm = -1;
		char* key = NULL;
		char* chord_type = NULL;
		int duration = -1;
		if (cJSON_IsNumber(bpm_json)) bpm = bpm_json->valueint;
		if (cJSON_IsString(key_json)) key = string_alloc(key_json->valuestring, string_length(key_json->valuestring));
		if (cJSON_IsString(chord_type_json)) chord_type = string_alloc(chord_type_json->valuestring, string_length(chord_type_json->valuestring));
		if (cJSON_IsNumber(duration_json)) duration = duration_json->valueint;

		curr_page->items[i] = (struct search_item){
			.session = session,
			.data = (struct search_item_data){
				.name = name,
				.name_full = name_full,
				.audio_url = audio_url,
				.bpm = bpm,
				.key = key,
				.chord_type = chord_type,
				.duration = duration,
				.tags = tags,
				.tags_length = tags_length,
			},
		};
		search_item_update(&curr_page->items[i]);
	}

	success = true;

cleanup:
	free(response.data);
	free(search_body);
	cJSON_Delete(response_json);
	cJSON_Delete(search_body_json);
	curl_slist_free_all(headers);
	curl_easy_cleanup(list_request);
	return success;
}

bool search_item_update(struct search_item* item)
{
	if (!item) return false;

	return item->session->context->item_update_func(item->session->context->db, &item->data);
}
