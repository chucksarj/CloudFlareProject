/** Cloudflare Summer Internship worker project
 *  Arjun Loganathan, arlogana@ucsc.edu 
 * */

// our template link array for json object and static html page
const link_arr = [
    {
        "name": "Google",
        "url": "https://www.google.com"
    },
    {
        "name": "Facebook",
        "url": "https://www.facebook.com"
    },
    {
        "name": "Reddit",
        "url": "https://www.reddit.com"
    }
]

// handle formatting for links div 
const LinksFMT = {
    element: (element) => {
        var v1 = 'name'
        var v2 = 'url'
        element.append(`<a href = \"${link_arr[0][v2]}\">${link_arr[0][v1]}</a>\n`, {html: Boolean})
        element.append(`<a href = \"${link_arr[1][v2]}\">${link_arr[1][v1]}</a>\n`, {html: Boolean})
        element.append(`<a href = \"${link_arr[2][v2]}\">${link_arr[2][v1]}</a>\n`, {html: Boolean})
    },
}

// handle formatting for profile div
const ProfileFMT = {
    element: (element) => {
        element.removeAttribute('style')
    },
}

const ImgFMT = {
    element: (element) => {
        element.setAttribute('src', 'https://i.ibb.co/SftfNgG/prof.jpg')
    },
}

const TitleFMT = {
    element: (element) => {
        element.setInnerContent('arlogana')
    },
}

const SocialFMT = {
    element: (element) => {
        element.removeAttribute('style')
        element.append('<a href = \"https://github.com/chucksarj\"><svg><img src = \"https://cdn.jsdelivr.net/npm/simple-icons@v3/icons/github.svg\" /></svg></a>\n', {html: Boolean})
        element.append('<a href = \"https://www.linkedin.com/in/arjun-l-57a819120/\"><svg><img src = \"https://cdn.jsdelivr.net/npm/simple-icons@v3/icons/linkedin.svg\" /></svg></a>\n', {html: Boolean})
    },
}

const NameFMT = {
    element: (element) => {
        element.setInnerContent('Arjun Loganathan')
    },
}

const BodyFMT = {
    element: (element) => {
        element.removeAttribute('class')
        element.setAttribute('style', "background-image: url('https://i.ibb.co/5BvqH1s/Huge-Backgrounds-62.jpg'); background-repeat: no-repeat; background-attachment: fixed; background-size: cover;")
    },
}

addEventListener('fetch', event => {
    const my_url = new URL(event.request.url);
    var str = my_url.pathname;

    if (str == "/links") {
        event.respondWith(handleRequest(event.request))
    } else {
        event.respondWith(staticRequest())
    }
})

async function handleRequest(request) { 
    
    const json = JSON.stringify(link_arr, null, 4)

    return new Response(json, {
        headers: { 
            "content-type": "application/json;charset=UTF-8"
        }
    })
} 

const rewriter = new HTMLRewriter()
    .on('div#links', LinksFMT)
    .on('div#profile', ProfileFMT)
    .on('img#avatar', ImgFMT)
    .on('h1#name', TitleFMT)
    .on('div#social', SocialFMT)
    .on('title', NameFMT)
    .on('body', BodyFMT)

async function staticRequest() {

    const url = "https://static-links-page.signalnerve.workers.dev"

    const response = await fetch(url)
    
    return rewriter.transform(response)
}

