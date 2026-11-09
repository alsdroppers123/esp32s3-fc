package com.alsdroppers.esp32s3fc.fly

import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import java.net.ConnectException
import java.net.HttpURLConnection
import java.net.SocketTimeoutException
import java.net.URL
import java.net.UnknownHostException

/**
 * FC Fly: full-screen landscape wrapper around the board's /fly page (dual sticks + horizon HUD).
 *  - binds the process to the Wi-Fi network, so Android does not route around a hotspot that has no internet
 *  - keeps the screen on, hides the system bars, locks navigation to the board's address
 *  - the FC lands the drone if the link is silent for ~0.5 s: keep this app in the foreground while flying
 */
class MainActivity : Activity() {

    private val defaultHost = "192.168.4.1"
    private lateinit var web: WebView
    private lateinit var connectView: LinearLayout
    private lateinit var hostInput: EditText
    private lateinit var status: TextView
    private lateinit var goBtn: Button
    private var host: String? = null
    private var netCallback: ConnectivityManager.NetworkCallback? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val root = FrameLayout(this)
        root.setBackgroundColor(Color.parseColor("#0b0f14"))

        web = WebView(this)
        web.visibility = View.GONE
        web.setBackgroundColor(Color.BLACK)
        web.settings.javaScriptEnabled = true
        web.settings.domStorageEnabled = true
        web.settings.mediaPlaybackRequiresUserGesture = false
        web.settings.cacheMode = WebSettings.LOAD_NO_CACHE            // always the page of the flashed firmware
        web.webViewClient = object : WebViewClient() {
            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
                val u = request.url
                return !(u.scheme == "http" && u.authority?.lowercase() == host)
            }

            override fun onReceivedError(view: WebView, request: WebResourceRequest, error: WebResourceError) {
                if (request.isForMainFrame && host != null) {
                    runOnUiThread { showConnect("Lost the connection to $host (${error.description}).") }
                }
            }
        }
        root.addView(web, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)

        connectView = buildConnectView()
        root.addView(connectView, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT, Gravity.CENTER))

        setContentView(root)
        hostInput.setText(getPreferences(MODE_PRIVATE).getString("host", defaultHost))
        bindToWifi()
    }

    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()

    private fun buildConnectView(): LinearLayout {
        val box = LinearLayout(this)
        box.orientation = LinearLayout.VERTICAL
        box.gravity = Gravity.CENTER
        box.setPadding(dp(32), dp(16), dp(32), dp(16))

        fun text(s: String, size: Float, color: String): TextView {
            val t = TextView(this)
            t.text = s; t.textSize = size; t.setTextColor(Color.parseColor(color)); t.gravity = Gravity.CENTER
            return t
        }
        box.addView(text("FC Fly", 22f, "#e6edf3"))
        box.addView(text("Join the flight controller's Wi-Fi, then connect.", 13f, "#8b98a8"))

        hostInput = EditText(this)
        hostInput.setSingleLine(true)
        hostInput.hint = defaultHost
        hostInput.setTextColor(Color.parseColor("#e6edf3"))
        hostInput.setHintTextColor(Color.parseColor("#5b6877"))
        hostInput.imeOptions = EditorInfo.IME_ACTION_GO
        hostInput.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_GO) { connect(); true } else false
        }
        box.addView(hostInput, LinearLayout.LayoutParams(dp(300), ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(16) })

        goBtn = Button(this)
        goBtn.text = "Connect"
        goBtn.setOnClickListener { connect() }
        box.addView(goBtn, LinearLayout.LayoutParams(dp(300), ViewGroup.LayoutParams.WRAP_CONTENT))

        status = text("", 13f, "#8b98a8")
        box.addView(status, LinearLayout.LayoutParams(dp(420), ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(8) })
        return box
    }

    private fun setStatus(msg: String, error: Boolean) {
        status.text = msg
        status.setTextColor(Color.parseColor(if (error) "#ff6b6b" else "#8b98a8"))
    }

    // "192.168.4.1", "esp32fc.local", "http://192.168.4.1/", "192.168.4.1:8080" -> "host[:port]" or null
    private fun normalizeHost(input: String): String? {
        val s = input.trim().replace(Regex("^https?://", RegexOption.IGNORE_CASE), "").replace(Regex("/.*$"), "")
        if (s.isEmpty() || s.length > 255) return null
        val label = "[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?"
        val m = Regex("^($label(?:\\.$label)*)(?::(\\d{1,5}))?$").find(s) ?: return null
        val port = m.groupValues[2]
        if (port.isNotEmpty() && port.toInt() !in 1..65535) return null
        return m.groupValues[1].lowercase() + (if (port.isNotEmpty()) ":$port" else "")
    }

    // GET /api/net exists on the firmware; any 2xx means "this is the FC". Returns an error text or null.
    private fun probe(h: String): String? {
        var c: HttpURLConnection? = null
        return try {
            c = URL("http://$h/api/net").openConnection() as HttpURLConnection
            c.connectTimeout = 3000; c.readTimeout = 3000
            val code = c.responseCode
            if (code in 200..299) null else "The device answered with HTTP $code. Is this the flight controller?"
        } catch (e: UnknownHostException) {
            "Name not found. Try the IP address instead (hotspot default: $defaultHost)."
        } catch (e: SocketTimeoutException) {
            "No answer (timeout). Check that you are on the drone's Wi-Fi and the board is powered."
        } catch (e: ConnectException) {
            "Could not connect. Join the drone's Wi-Fi first, or check the address."
        } catch (e: Exception) {
            "Could not connect (${e.javaClass.simpleName})."
        } finally {
            c?.disconnect()
        }
    }

    private fun connect() {
        val h = normalizeHost(hostInput.text.toString())
        if (h == null) { setStatus("Enter an IP address or name, e.g. $defaultHost or esp32fc.local.", true); return }
        goBtn.isEnabled = false
        setStatus("Connecting...", false)
        Thread {
            val err = probe(h)
            runOnUiThread {
                goBtn.isEnabled = true
                if (err != null) setStatus(err, true) else {
                    getPreferences(MODE_PRIVATE).edit().putString("host", h).apply()
                    showFly(h)
                }
            }
        }.start()
    }

    private fun showFly(h: String) {
        host = h
        connectView.visibility = View.GONE
        web.visibility = View.VISIBLE
        hideSystemBars()
        web.loadUrl("http://$h/fly")
    }

    private fun showConnect(error: String?) {
        host = null
        web.visibility = View.GONE
        web.loadUrl("about:blank")
        connectView.visibility = View.VISIBLE
        if (error != null) setStatus(error, true) else setStatus("", false)
    }

    // Android may refuse to use (or silently drop) a Wi-Fi network without internet; bind this process to it.
    private fun bindToWifi() {
        try {
            val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            val req = NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build()
            val cb = object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) { cm.bindProcessToNetwork(network) }
                override fun onLost(network: Network) { cm.bindProcessToNetwork(null) }
            }
            cm.requestNetwork(req, cb)
            netCallback = cb
        } catch (e: Exception) {
            // not fatal: the normal routing still works when the hotspot is the active network
        }
    }

    @Suppress("DEPRECATION")
    private fun hideSystemBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            window.insetsController?.let {
                it.hide(WindowInsets.Type.systemBars())
                it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus && web.visibility == View.VISIBLE) hideSystemBars()
    }

    @Suppress("OVERRIDE_DEPRECATION", "DEPRECATION")
    override fun onBackPressed() {
        if (web.visibility == View.VISIBLE) showConnect(null) else super.onBackPressed()
    }

    override fun onPause() { super.onPause(); web.onPause() }
    override fun onResume() { super.onResume(); web.onResume() }

    override fun onDestroy() {
        try {
            val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            netCallback?.let { cm.unregisterNetworkCallback(it) }
            cm.bindProcessToNetwork(null)
        } catch (e: Exception) { /* ignore */ }
        web.destroy()
        super.onDestroy()
    }
}
