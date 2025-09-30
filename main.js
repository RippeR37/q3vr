// main.js — small site behaviors
// smooth scroll for internal links
document.addEventListener('DOMContentLoaded', function(){
  document.querySelectorAll('a[href^="#"]').forEach(function(anchor){
    anchor.addEventListener('click', function(e){
      var target = document.querySelector(this.getAttribute('href'));
      if(target){
        e.preventDefault();
        target.scrollIntoView({behavior:'smooth',block:'start'});
      }
    });
  });

  // small tweak: add active class on scroll for nav links
  var sections = Array.from(document.querySelectorAll('main section'));
  var navLinks = Array.from(document.querySelectorAll('.navbar .nav-link'));
  function onScroll(){
    var pos = window.scrollY + 120;
    var current = sections.find(function(s){
      var r = s.getBoundingClientRect();
      var top = window.scrollY + r.top;
      return top <= pos && top + s.offsetHeight > pos;
    });
    navLinks.forEach(function(l){ l.classList.remove('active'); });
    if(current){
      var id = '#' + current.id;
      var a = document.querySelector('.navbar .nav-link[href="'+id+'"]');
      if(a) a.classList.add('active');
    }
  }
  window.addEventListener('scroll', onScroll);
  onScroll();
});
