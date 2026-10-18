// Enable zoom on images larger than 300px
document.querySelectorAll('.content img').forEach(function (img) {
	var width = img.getAttribute('width');
	if (!width || width >= 300) {
		img.setAttribute('data-action', 'zoom');
	}
});
