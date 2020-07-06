
function eatlas_content_checker_check_urls(forHttps) {

  (function ($) {
    $("span.check-url").each(function() {
      var element = $(this);
      var url = element.text();
      if (forHttps) {
        url.replace("http://", "https://");
      }

      var proxyUrl = "/admin/config/eatlas/eatlas_content_checker/get_url_header?check-url=" + encodeURIComponent(url);

      $.ajax({
        "url": proxyUrl,
        "success": function(el, url) {
          return function(content) {
            var contentParts = content.split("\n");
            var status = parseInt(contentParts[0]);
            var redirectUrl = contentParts[1];

            if (status < 400) {
              el.css("color", "#009900");
            } else {
              el.css("color", "#990000");
            }

            var statusCodeLine = '';
            if (redirectUrl) {
              statusCodeLine += '<span class"redirect-url">' + redirectUrl + '</span> =&gt; '
            }
            statusCodeLine += '<span class"status-code">' + status + '</span>';
            el.append(' [' + statusCodeLine + ']');
          };
        }(element, url),
        "error": function() {
          console.error("ERROR occurred while sending a request to get_url_header.");
        }
      });
    });
  }(jQuery));
}
