<?php

function _eatlas_content_checker_overview() {
  $output = '<h2>' . t('eAtlas node issues') . '</h2>';

  $output .= '<p><a href="/admin/config/eatlas/eatlas_content_checker/offending_style">Check for offending CSS style</a></p>';
  $output .= '<p><a href="/admin/config/eatlas/eatlas_content_checker/broken_links">Check for broken URLs</a></p>';
  $output .= '<p><a href="/admin/config/eatlas/eatlas_content_checker/http_links">Check for HTTP URLs</a></p>';

  return $output;
}



function _eatlas_content_checker_offending_style() {
  set_time_limit(600); // 10 minutes

  $output = '<h2>' . t('Content checker - find nodes containing offending CSS style') . '</h2>';

  $offending_style_nodes = _eatlas_content_checker_list_offending_style_nodes();

  $header = array(t('Nodes'));
  $rows = array();
  foreach($offending_style_nodes as $node) {
    $rows[] = array(
      l($node->title, "node/$node->nid"),
    );
  }
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No issue found.')));

  return $output;
}
function _eatlas_content_checker_list_offending_style_nodes() {
  // List of common style that appear in the markup when doing copy/paste
  $nids = _eatlas_content_checker_get_all_node_ids(array(
    "line-height:",
    "letter-spacing:",
    "word-spacing:",
    "font-family:",
    "font-variant:",
    "font-style:",
    "text-decoration:",
    "text-indent:",
    "orphans:",
    "color: rgb(", // CKEditor tends to add text colour encoded in rgb colour
    "clear: both", // Float layout should be replaced with flex where possible (works better on mobile)
    "<span", // Most copied content are placed in span
    "<font", // Sometime, CKEditor will use the deprecated font tag
    "<h1", // H1 should not be used in articles. That's reserved for page title
    "<a>" // One article contained many empty "a" tag. Not sure if that's a common issue
  ));
  // Useful for testing
  $nids = array_slice($nids, 0, 50);

  $found = array();

  foreach ($nids as $nid){
    $node = node_load($nid);

    array_push($found, (object)array(
      "nid" => $node->nid,
      "title" => $node->title,
    ));
  }

  return $found;
}



function _eatlas_content_checker_broken_urls_list() {
  set_time_limit(600); // 10 minutes

  $output = '<h2>' . t('Content checker - find broken URLs') . '</h2>';

  $output .= '<button class="check_urls" onclick="eatlas_content_checker_check_urls()">Check URL http code</button>';

  // Replace with list of node which have issues
  $all_link_nodes = _eatlas_content_checker_list_all_link_nodes();

  $header = array(t('Nodes'), t('URLs'));
  $rows = array();
  foreach($all_link_nodes as $node) {
    $rows[] = array(
      l($node->title, "node/$node->nid"),
      _eatlas_content_checker_url_array_to_list($node->urls)
    );
  }
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No issue found.')));

  return $output;
}
function _eatlas_content_checker_list_all_link_nodes() {
  $nids = _eatlas_content_checker_get_all_node_ids(array("https://", "http://"));
  // Useful for testing
  //$nids = array_slice($nids, 0, 50);

  $found = array();

  foreach ($nids as $nid){
    $node = node_load($nid);
    $body = field_get_items('node', $node, 'body');
    $urls = _eatlas_content_checker_extract_urls($body[0]['value']);

    array_push($found, (object)array(
      "nid" => $node->nid,
      "title" => $node->title,
      "urls" => $urls
    ));
  }

  return $found;
}



function _eatlas_content_checker_http_urls_list() {
  set_time_limit(600); // 10 minutes
  drupal_add_js(drupal_get_path('module', 'eatlas_content_checker') . '/eatlas_content_checker.js');

  $output = '<h2>' . t('Content checker - find HTTP URLs that could be replaced with HTTPS') . '</h2>';

  $output .= '<button class="check_urls" onclick="eatlas_content_checker_check_urls(true)">Check which URL can be changed to HTTPS</button>';

  // Replace with list of node which have issues
  $http_nodes = _eatlas_content_checker_list_http_nodes();

  $header = array(t('Nodes'), t('URLs'));
  $rows = array();
  foreach($http_nodes as $node) {
    $rows[] = array(
      l($node->title, "node/$node->nid"),
      _eatlas_content_checker_url_array_to_list($node->urls)
    );
  }
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No issue found.')));

  return $output;
}
function _eatlas_content_checker_list_http_nodes() {
  $nids = _eatlas_content_checker_get_all_node_ids(array("http://"));

  $found = array();

  foreach ($nids as $nid){
    $node = node_load($nid);
    $body = field_get_items('node', $node, 'body');
    $urls = _eatlas_content_checker_extract_http_urls($body[0]['value']);

    array_push($found, (object)array(
      "nid" => $node->nid,
      "title" => $node->title,
      "urls" => $urls
    ));
  }

  return $found;
}



function _eatlas_content_checker_url_array_to_list($url_array) {
  $url_list = '<ul>';
  foreach($url_array as $url) {
    $url_list .= '<li><span class="check-url">' . $url . '</span></li>';
  }
  $url_list .= '</ul>';
  return $url_list;
}

function _eatlas_content_checker_get_all_node_ids($contains_array) {
  // Return all nids of nodes of type "page".
  $query = db_select('field_data_body', 'b')
    ->fields('b', array('entity_id'))
    ->condition('b.entity_type', 'node')
    ->condition('b.bundle', array('external_content', 'eatlas_georss_item'), 'NOT IN')
    ->condition('b.deleted', 0);

  if ($contains_array) {
    $contains_array_length = count($contains_array);

    if ($contains_array_length > 1) {
      $or = db_or();
      foreach($contains_array as $contains_el) {
        $or->condition('b.body_value', '%' . db_like($contains_el) . '%', 'LIKE');
      }
      $query->condition($or);

    } elseif ($contains_array_length == 1) {
      $query = $query->condition('b.body_value', '%' . db_like($contains_array[0]) . '%', 'LIKE');
    }
  }

  return array_values($query->distinct()
    ->execute()
    ->fetchCol() // returns an indexed array
  );
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


// Basically an open reverse proxy for HTTP Headers
function _eatlas_content_checker_get_url_header() {
  $params = drupal_get_query_parameters();
  $url = $params["check-url"];

  $curl = curl_init($url);
  curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
  curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($curl, CURLOPT_HEADER, true);
  curl_setopt($curl, CURLOPT_NOBODY, true);
  // Connection timeout to 30 sec (url with wrong port for example)
  curl_setopt($curl, CURLOPT_CONNECTTIMEOUT, 30);
  // Request timeout (the server is too slow)
  curl_setopt($curl, CURLOPT_TIMEOUT, 30);

  $header = curl_exec($curl);

  // First line: http code
  $http_code = curl_getinfo($curl, CURLINFO_HTTP_CODE);
  if ($http_code === 0) {
    $http_code = 404;
  }
  print $http_code . PHP_EOL;

  // Second line: redirection URL if any
  $redirect_url = curl_getinfo($curl, CURLINFO_EFFECTIVE_URL);
  if ($redirect_url !== $url) {
    print $redirect_url;
  }

  curl_close($curl);
}
?>
