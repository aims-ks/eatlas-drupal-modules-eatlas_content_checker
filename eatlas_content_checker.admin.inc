<?php

function _eatlas_content_checker_overview() {
  $output = '<h2>' . t('eAtlas node issues') . '</h2>';

  $output .= '<p><a href="/admin/config/eatlas/eatlas_content_checker/broken_links">Check for broken URLs</a></p>';
  $output .= '<p><a href="/admin/config/eatlas/eatlas_content_checker/http_links">Check for HTTP URLs</a></p>';

  return $output;
}

function _eatlas_content_checker_broken_urls_list() {
  set_time_limit(3600); // 1 hour

  $output = '<h2>' . t('Broken links') . '</h2>';

  // Replace with list of node which have issues
  $broken_link_nodes = _eatlas_content_checker_list_broken_link_nodes();

  $header = array(t('Nodes'), t('Broken URLs'));
  $rows = array();
  foreach($broken_link_nodes as $node) {
    $rows[] = array(
      l($node->title, "node/$node->nid"),
      implode("<br/>", $node->broken_urls)
    );
  }
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No issue found.')));

  return $output;
}

function _eatlas_content_checker_http_urls_list() {
  $output = '<h2>' . t('HTTP URLs that could be replaced with HTTPS') . '</h2>';

  // Replace with list of node which have issues
  $http_nodes = _eatlas_content_checker_list_http_nodes();

  $header = array(t('Nodes'), t('URLs'));
  $rows = array();
  foreach($http_nodes as $node) {
    $rows[] = array(
      l($node->title, "node/$node->nid"),
      implode("<br/>", $node->urls)
    );
  }
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No issue found.')));

  return $output;
}

function _eatlas_content_checker_list_broken_link_nodes() {
  $nids = _eatlas_content_checker_get_all_node_ids("http");

  // TEMPORARY!!
  $nids = array_slice($nids, 0, 50);

  $node_url_status_map = _eatlas_content_checker_get_node_url_status_map($nids);

  $nodes = array();
  foreach ($node_url_status_map as $nid => $url_status_map){
    $node = node_load($nid);

    $broken_urls = array();
    foreach($url_status_map as $url => $url_status) {
      array_push($broken_urls, $url . ' [' . $url_status['http_code'] . ']');
    }

    array_push($nodes, (object)array(
      "nid" => $node->nid,
      "title" => $node->title,
      "broken_urls" => $broken_urls
    ));
  }

  return $nodes;
}

function _eatlas_content_checker_list_http_nodes() {
  $nids = _eatlas_content_checker_get_all_node_ids("http://");

  $found = array();

  foreach ($nids as $nid){
    $node = node_load($nid);
    $body = field_get_items('node', $node, 'body');
    $urls = _eatlas_content_checker_extract_http_urls($body[0]['value']);

    $broken_urls = array();
    foreach($urls as $url) {
      // if can be replaced with https
      array_push($broken_urls, $url);
    }

    array_push($found, (object)array(
      "nid" => $node->nid,
      "title" => $node->title,
      "urls" => $urls
    ));
  }

  return $found;
}

function _eatlas_content_checker_get_all_node_ids($contains) {
  // Return all nids of nodes of type "page".
  $query = db_select('field_data_body', 'b')
    ->fields('b', array('entity_id'))
    ->condition('b.entity_type', 'node')
    ->condition('b.deleted', 0);

  if ($contains) {
    $query = $query->condition('b.body_value', '%' . db_like($contains) . '%', 'LIKE');
  }

  return array_values($query->distinct()
    ->execute()
    ->fetchCol() // returns an indexed array
  );
}

// Parameters:
//    $nids = Array of node ID
//    $check_https = Boolean. True to also check HTTPS version of the URL if the URL is HTTP
// Returns:
//   Map
//     key: nid
//     value: Map
//       key: URL
//       value: Object
//         "url": URL
//         "redirect_url": URL (if redirected)
//         "http_code": code
function _eatlas_content_checker_get_node_url_status_map($nids, $check_https = false) {

  // Create a map of "url" => array nid
  $nid_url_map = array();
  $url_map = array();
  foreach ($nids as $nid){
    $node = node_load($nid);
    $body = field_get_items('node', $node, 'body');
    $urls = _eatlas_content_checker_extract_urls($body[0]['value']);
    $nid_url_map[$nid] = $urls;
    foreach($urls as $url) {
      if (!array_key_exists($url, $url_map)) {
        $url_map[$url] = true;
      }
    }
  }

  $url_http_codes = _eatlas_content_checker_get_http_headers(array_keys($url_map));

  // Build the return map
  $return_map = array();
  foreach ($nid_url_map as $nid => $urls) {
    $urls_status = array();
    foreach ($urls as $url) {
      $urls_status[$url] = $url_http_codes[$url];
    }
    $return_map[$nid] = $urls_status;
  }

  return $return_map;
}

// Inspired from:
//  https://stackoverflow.com/questions/36564293/extract-urls-from-a-string-using-php#answer-36564776
function _eatlas_content_checker_extract_urls($body) {
  preg_match_all('#\bhttps?://[^,\s()<>]+(?:\([\w\d]+\)|([^,[:punct:]\s]|/))#', $body, $match);
  return _eatlas_content_checker_remove_duplicates($match[0]);
}
function _eatlas_content_checker_extract_http_urls($body) {
  preg_match_all('#\bhttp://[^,\s()<>]+(?:\([\w\d]+\)|([^,[:punct:]\s]|/))#', $body, $match);
  return _eatlas_content_checker_remove_duplicates($match[0]);
}

function _eatlas_content_checker_remove_duplicates($array) {
  $unique = array();
  foreach ($array as $element) {
    $unique[$element] = true;
  }
  return array_keys($unique);
}

// Inspired from:
//   https://stackoverflow.com/questions/9308779/php-parallel-curl-requests#answer-9311112
// Return:
//   Map
//     Key:
//       URL
//     Value:
//       Object:
//         "url": URL
//         "redirect_url": URL (if redirected)
//         "http_code": code
function _eatlas_content_checker_get_http_headers($url_array) {
  $curl_multi = curl_multi_init();
  $curl_array = array();

  foreach ($url_array as $url) {
    $curl = curl_init($url);
    // https://www.php.net/manual/en/function.curl-setopt.php
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_HEADER, true);
    curl_setopt($curl, CURLOPT_NOBODY, true);
    curl_multi_add_handle($curl_multi, $curl);
    array_push($curl_array, $curl);
  }

  do {
    curl_multi_exec($curl_multi,$running);
  } while ($running > 0);

  $results = array();
  for ($i=0; $i<count($url_array); $i++) {
    $url = $url_array[$i];
    $curl = $curl_array[$i];
    $redirect_url = curl_getinfo($curl, CURLINFO_EFFECTIVE_URL);
    if ($url === $redirect_url) {
      $redirect_url = null;
    }
    $results[$url] = array(
      "url" => $url,
      "redirect_url" => $redirect_url,
      "http_code" => curl_getinfo($curl, CURLINFO_HTTP_CODE)
    );
    curl_close($curl);
  }

  return $results;
}

?>
