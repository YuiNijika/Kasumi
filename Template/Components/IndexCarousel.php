<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
$CarouselHeight = Get::Options('CarouselHeight') ? Get::Options('CarouselHeight') : '340px';
$CarouselContent = Get::Options('CarouselContent');

// 解析 CarouselContent
$carouselItems = [];
if ($CarouselContent) {
    $lines = explode("\n", $CarouselContent);
    foreach ($lines as $line) {
        $parts = explode('|', $line);
        if (count($parts) == 2) {
            $carouselItems[] = [
                'link' => trim($parts[0]),
                'image' => trim($parts[1]),
            ];
        }
    }
}
?>
<a-carousel
    class="KasumiCarousel"
    :auto-play="true"
    animation-name="fade"
    indicator-type="dot"
    show-arrow="hover"
    :style="{height: '<?php echo $CarouselHeight; ?>'}">
    <?php foreach ($carouselItems as $item): ?>
        <a-carousel-item class="KasumiCarouselItem">
            <a href="<?php echo htmlspecialchars($item['link']); ?>" target="_blank">
                <img src="<?php echo htmlspecialchars($item['image']); ?>" />
            </a>
        </a-carousel-item>
    <?php endforeach; ?>
</a-carousel>