-- BetterPython Neovim Configuration
-- Complete LSP and TreeSitter integration

local M = {}

-- LSP Configuration
function M.setup_lsp()
  local lspconfig = require('lspconfig')
  local configs = require('lspconfig.configs')
  
  -- Define BetterPython LSP configuration
  if not configs.betterpython then
    configs.betterpython = {
      default_config = {
        cmd = {'python3', vim.fn.stdpath('data') .. '/betterpython/lsp-server/betterpython_lsp.py'},
        filetypes = {'betterpython'},
        root_dir = function(fname)
          return lspconfig.util.find_git_ancestor(fname) or vim.fn.getcwd()
        end,
        settings = {},
        init_options = {},
      },
    }
  end
  
  -- Start LSP server
  lspconfig.betterpython.setup({
    on_attach = function(client, bufnr)
      -- Enable completion
      vim.api.nvim_buf_set_option(bufnr, 'omnifunc', 'v:lua.vim.lsp.omnifunc')
      
      -- Key mappings
      local opts = { noremap=true, silent=true, buffer=bufnr }
      
      vim.keymap.set('n', 'gD', vim.lsp.buf.declaration, opts)
      vim.keymap.set('n', 'gd', vim.lsp.buf.definition, opts)
      vim.keymap.set('n', 'K', vim.lsp.buf.hover, opts)
      vim.keymap.set('n', 'gi', vim.lsp.buf.implementation, opts)
      vim.keymap.set('n', '<C-k>', vim.lsp.buf.signature_help, opts)
      vim.keymap.set('n', '<space>wa', vim.lsp.buf.add_workspace_folder, opts)
      vim.keymap.set('n', '<space>wr', vim.lsp.buf.remove_workspace_folder, opts)
      vim.keymap.set('n', '<space>wl', function()
        print(vim.inspect(vim.lsp.buf.list_workspace_folders()))
      end, opts)
      vim.keymap.set('n', '<space>D', vim.lsp.buf.type_definition, opts)
      vim.keymap.set('n', '<space>rn', vim.lsp.buf.rename, opts)
      vim.keymap.set('n', '<space>ca', vim.lsp.buf.code_action, opts)
      vim.keymap.set('n', 'gr', vim.lsp.buf.references, opts)
      vim.keymap.set('n', '<space>f', function()
        vim.lsp.buf.format { async = true }
      end, opts)
      
      -- Diagnostic keymaps
      vim.keymap.set('n', '<space>e', vim.diagnostic.open_float, opts)
      vim.keymap.set('n', '[d', vim.diagnostic.goto_prev, opts)
      vim.keymap.set('n', ']d', vim.diagnostic.goto_next, opts)
      vim.keymap.set('n', '<space>q', vim.diagnostic.setloclist, opts)
    end,
    capabilities = require('cmp_nvim_lsp').default_capabilities(),
  })
end

-- TreeSitter Configuration
function M.setup_treesitter()
  require('nvim-treesitter.configs').setup({
    ensure_installed = { "betterpython" },
    
    highlight = {
      enable = true,
      additional_vim_regex_highlighting = false,
    },
    
    indent = {
      enable = true,
    },
    
    incremental_selection = {
      enable = true,
      keymaps = {
        init_selection = "gnn",
        node_incremental = "grn",
        scope_incremental = "grc",
        node_decremental = "grm",
      },
    },
    
    textobjects = {
      select = {
        enable = true,
        lookahead = true,
        keymaps = {
          ["af"] = "@function.outer",
          ["if"] = "@function.inner",
          ["ac"] = "@class.outer",
          ["ic"] = "@class.inner",
        },
      },
    },
  })
end

-- Autocommands for BetterPython files
function M.setup_autocommands()
  local group = vim.api.nvim_create_augroup('BetterPython', { clear = true })
  
  -- Set filetype
  vim.api.nvim_create_autocmd({'BufRead', 'BufNewFile'}, {
    group = group,
    pattern = '*.bp',
    callback = function()
      vim.bo.filetype = 'betterpython'
    end,
  })
  
  -- Format on save (optional)
  vim.api.nvim_create_autocmd('BufWritePre', {
    group = group,
    pattern = '*.bp',
    callback = function()
      if vim.g.betterpython_format_on_save then
        vim.lsp.buf.format({ async = false })
      end
    end,
  })
  
  -- Lint on save
  vim.api.nvim_create_autocmd('BufWritePost', {
    group = group,
    pattern = '*.bp',
    callback = function()
      if vim.g.betterpython_lint_on_save then
        vim.fn.system('bplint ' .. vim.fn.expand('%'))
      end
    end,
  })
end

-- Commands
function M.setup_commands()
  -- Run current file
  vim.api.nvim_create_user_command('BPRun', function()
    local file = vim.fn.expand('%:p')
    vim.cmd('vsplit | terminal betterpython ' .. file)
  end, {})
  
  -- Compile current file
  vim.api.nvim_create_user_command('BPCompile', function()
    local file = vim.fn.expand('%:p')
    local output = vim.fn.expand('%:p:r') .. '.bpc'
    vim.fn.system('bpcc ' .. file .. ' -o ' .. output)
    print('Compiled to ' .. output)
  end, {})
  
  -- Format current file
  vim.api.nvim_create_user_command('BPFormat', function()
    local file = vim.fn.expand('%:p')
    vim.fn.system('bpfmt ' .. file)
    vim.cmd('edit!')
  end, {})
  
  -- Lint current file
  vim.api.nvim_create_user_command('BPLint', function()
    local file = vim.fn.expand('%:p')
    local output = vim.fn.system('bplint ' .. file)
    print(output)
  end, {})
end

-- Key mappings
function M.setup_keymaps()
  vim.api.nvim_create_autocmd('FileType', {
    pattern = 'betterpython',
    callback = function()
      local opts = { noremap = true, silent = true, buffer = true }
      
      -- F5 to run
      vim.keymap.set('n', '<F5>', ':BPRun<CR>', opts)
      
      -- Leader mappings
      vim.keymap.set('n', '<leader>bc', ':BPCompile<CR>', opts)
      vim.keymap.set('n', '<leader>bf', ':BPFormat<CR>', opts)
      vim.keymap.set('n', '<leader>bl', ':BPLint<CR>', opts)
    end,
  })
end

-- Main setup function
function M.setup()
  M.setup_lsp()
  M.setup_treesitter()
  M.setup_autocommands()
  M.setup_commands()
  M.setup_keymaps()
  
  -- Set globals
  vim.g.betterpython_format_on_save = false
  vim.g.betterpython_lint_on_save = true
end

return M
