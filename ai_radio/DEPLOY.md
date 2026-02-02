# Deployment Guide: Keegan Radioverse

This guide covers deploying the **Registry Server** (Backend) and the **Web Console** (Frontend).

## Prerequisites
- [GitHub Account](https://github.com) (Repo pushed to GitHub)
- [Render Account](https://render.com) (for Backend)
- [Vercel Account](https://vercel.com) (for Frontend)

---

## Part 1: Backend (Render)
*Hosts the Station Registry and Telemetry.*

1.  **Dashboard**: Go to [dashboard.render.com](https://dashboard.render.com/) -> **New +** -> **Web Service**.
2.  **Connect Repo**: Select your `Keegan` repository.
3.  **Settings**:
    - **Name**: `keegan-registry`
    - **Region**: US East (or closest to you)
    - **Root Directory**: `ai_radio`
    - **Runtime**: **Python 3**
    - **Build Command**: `pip install -r requirements.txt` (it's empty now, but good practice)
    - **Start Command**: `python server/registry_server.py` (or leave blank, it reads the `Procfile`)
    - **Plan**: Free
4.  **Environment Variables**:
    - `ALLOWED_ORIGINS`: `*` (TEMPORARY: allows any Vercel URL to connect. Tighten this later once you have your Vercel URL).
    - `KEEGAN_REGISTRY_KEY`: `secret_admin_key_123` (Set your own secret for admin actions).
    - `PYTHONUNBUFFERED`: `true` (Ensures logs show up immediately).
    - `PORT`: `10000` (Render's default port).
5.  **Deploy**: Click **Create Web Service**.
6.  **Copy URL**: Once live, copy the URL (e.g., `https://keegan-registry.onrender.com`).

---

## Part 2: Frontend (Vercel)
*Hosts the React Web UI.*

1.  **Dashboard**: Go to [vercel.com/new](https://vercel.com/new).
2.  **Import**: Select your `Keegan` repository.
3.  **Configure Project**:
    - **Framework Preset**: Vite
    - **Root Directory**: Click "Edit" and select `web`. **(Crucial Step)**
4.  **Environment Variables**:
    - `VITE_REGISTRY_URL`: Paste your Render URL (e.g., `https://keegan-registry.onrender.com`). **Do not add a trailing slash.**
5.  **Deploy**: Click **Deploy**.

---

## Part 3: Verify & Tighten Security

1.  **Test**: Open your new Vercel URL. You should see the Console. The "Regional Directory" should load (it might be empty initially).
2.  **Tighten Backend**:
    - Go back to **Render Dashboard** -> **Environment**.
    - Edit `ALLOWED_ORIGINS`.
    - Set it to your Vercel URL (e.g., `https://keegan-radio.vercel.app`).
    - Save Changes.

## Part 4: Connect Your Desktop Engine
To make your local Keegan.exe report to this cloud registry:

1.  Open `./src/config.h` (or generic configuration headers).
2.  Update the `DEFAULT_REGISTRY_URL` or use the command line args (if implemented).
3.  *Temporary*: The current C++ engine might be hardcoded to localhost or a config file. Check `config/station.json` (if exists) or create one with:
    ```json
    {
      "registryUrl": "https://keegan-registry.onrender.com"
    }
    ```
